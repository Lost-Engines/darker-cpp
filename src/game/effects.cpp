#include "game/effects.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include "game/effect_tables.h"
#include "game/random.h"
#include "maths/world_coordinates.h"

namespace darker::game {

std::optional<particle_frame> particle_phase(particle_emitter const &emitter, clock_tick const clock) noexcept {
  /// 6C01/6C65 age backwards, retaining phase zero at the exact final tick
  auto const elapsed{std::bit_cast<int16_t>(static_cast<uint16_t>(clock - emitter.start))};
  if(elapsed < 0) return std::nullopt;
  int const remaining{static_cast<int>(emitter.animation.lifetime_ticks()) - elapsed};
  if(remaining < 0) return std::nullopt;
  auto phase{static_cast<uint8_t>(remaining / emitter_animation::ticks_per_phase)};
  uint8_t constexpr fire_transition_phase{10};
  uint8_t constexpr fire_frame_offset{6};                                      // upper fire phases skip the six intervening smoke frames
  if(emitter.animation.mode() == particle_animation_mode::fire && phase >= fire_transition_phase) phase += fire_frame_offset;
  return phase;
}

void advance_emitter(particle_emitter &emitter, clock_tick const clock, game_duration const step) noexcept {
  /// 6CD8 preserves signed byte rates and carries the fractional height into the wrapping word
  if(std::bit_cast<int16_t>(static_cast<uint16_t>(clock - emitter.start)) < 0) return;
  auto const ticks{std::bit_cast<int8_t>(static_cast<uint8_t>(step))};
  int const delta{emitter.height_rate * ticks};
  unsigned int const fraction{static_cast<unsigned int>(emitter.height_fraction) + static_cast<uint8_t>(delta * 32)};
  emitter.height_fraction = static_cast<uint8_t>(fraction);
  emitter.position.height = static_cast<uint16_t>(emitter.position.height + (delta >> 3) + (fraction >> 8));
  emitter.radius = static_cast<uint16_t>(emitter.radius + emitter.radius_rate * ticks);
  emitter.angle = static_cast<uint16_t>(emitter.angle + emitter.angle_rate * ticks);
}

std::optional<damage_severity> damage_trail_severity(uint16_t const damage, uint8_t const flags, uint16_t const changes) noexcept {
  /// 79C2/7A39 gate trails using damage high-byte bands and accumulated timer transitions
  if(flags & 0x18) return std::nullopt;
  auto const level{damage >> 8};
  if(level < 128) return std::nullopt;
  auto const mask{level < 160 ? 0x400 >> ((level - 128) >> 3) : 0x40};
  if(!(mask & changes)) return std::nullopt;
  return static_cast<uint8_t>(level < 160 ? 0 : std::min(level - 160, 63));
}

particle_emitter make_damage_trail(maths::world_position position, damage_severity const severity, uint16_t const random, clock_tick const clock) noexcept {
  /// 676E jitters the saved position using one original random word, then leaves the sprite stationary
  uint8_t constexpr base_lifetime_phases{8};                                   // even the lightest damage trail starts with eight animation phases
  position.row = static_cast<uint16_t>(position.row + (std::bit_cast<int16_t>(random) >> 11));
  auto const shifted{static_cast<uint16_t>(random << 5)};
  auto const dx{std::bit_cast<int16_t>(shifted) >> 11};
  position.column = static_cast<uint16_t>(position.column + dx);
  position.height = static_cast<uint16_t>(position.height + std::bit_cast<int8_t>(static_cast<uint8_t>(random * 4)));
  return {
    .position{position},
    .start{clock},
    .animation{emitter_animation{static_cast<uint8_t>(emitter_animation::fire_flag + base_lifetime_phases + (severity >> 3))}}
  };
}

void effect_system::spawn(uint16_t const recipe, maths::world_position const position, clock_tick const clock) {
  /// 6820 expands the executable recipe into independently delayed, moving emitter records
  auto const found{std::ranges::find(original_effect_recipes, recipe, &effect_recipe::address)};
  if(found == original_effect_recipes.end()) throw std::invalid_argument{"Unknown effect recipe"};
  for(auto const &source : found->emitters) {
    particle_emitter emitter{
      .position{position},
      .start{static_cast<uint16_t>(clock + source.delay)},
      .radius{source.radius},
      .angle{source.angle},
      .sampling{source.sampling},
      .radius_rate{source.radius_rate},
      .height_rate{source.height_rate},
      .angle_rate{source.angle_rate},
      .animation{emitter_animation{source.flags}},
    };
    for(unsigned int axis{0}; axis < 3; ++axis) emitter.position[axis] = static_cast<uint16_t>(emitter.position[axis] + source.offset[axis]);
    if(emitters.size() == emitter_capacity) emitters.erase(emitters.begin());
    emitters.push_back(emitter);
  }
  for(auto const &sound : found->sounds) {
    effect_sounds.append({
      .position{position},
      .definition{sound},
      .deadline{static_cast<uint16_t>(clock + sound.duration)}
    });
  }
}

void effect_system::spark(maths::world_position const position, uint8_t const phase, uint16_t const sound_level, clock_tick const clock) {
  /// 6742 emits a stationary sprite plus a short impact sound, also used by the Wrecker's cutting effects
  uint8_t constexpr impact_sound_patch{22};                                    // index into the game's FM timbre bank, not a MIDI program
  uint16_t constexpr impact_sound_duration{256};                               // native timer ticks; sprite lifetime is independent
  uint16_t constexpr impact_sound_pitch{0x203};                                // native driver pitch code before spatial/Doppler adjustment
  if(trails.size() == trail_capacity) trails.erase(trails.begin());
  trails.push_back({
    .position{position},
    .start{clock},
    .animation{emitter_animation{phase}}
  });
  impact_sounds.append({
    .position{position},
    .definition{
      .duration{impact_sound_duration},
      .pitch{impact_sound_pitch},
      .level{sound_level},
      .patch{impact_sound_patch},
      .flags{effect_sound_definition::spatial_stereo_flag}
    },
    .deadline{static_cast<uint16_t>(clock + impact_sound_duration)}
  });
}

void effect_system::gun_impact(maths::world_position position, bool const hit, clock_tick const clock) {
  /// 6730/6742 create a short endpoint sprite and an independently timed impact sound
  uint16_t constexpr height_alignment_mask{0xfff8};                            // clear the lowest three height bits: snap down to an eight-unit boundary
  uint8_t constexpr hit_lifetime_phases{3};                                    // 192 native ticks, counting backwards through sprite phases 3 to 0
  uint8_t constexpr miss_lifetime_phases{6};                                   // 384 native ticks, counting backwards through sprite phases 6 to 0
  uint16_t constexpr hit_sound_level{0xde30};                                  // source level before distance and stereo attenuation
  uint16_t constexpr miss_sound_level{0xce30};                                 // quieter than a confirmed hit
  position.height &= height_alignment_mask;
  spark(position, hit ? hit_lifetime_phases : miss_lifetime_phases, hit ? hit_sound_level : miss_sound_level, clock);
}

void effect_system::trail(maths::world_position const position, damage_severity const severity, uint16_t &random, clock_tick const clock) {
  /// Advance the shared random sequence only when a trail is emitted
  if(trails.size() == trail_capacity) trails.erase(trails.begin());
  trails.push_back(make_damage_trail(position, severity, next_random(random), clock));
}

void effect_system::advance(clock_tick const clock, game_duration const step) {
  /// Remove expired effects before movement; delayed records retain their initial state
  auto const expired{[&](auto const &emitter){
    return std::bit_cast<int16_t>(static_cast<uint16_t>(clock - emitter.start)) > static_cast<int>(emitter.animation.lifetime_ticks());
  }};
  auto const sound_expired{[&](auto const &sound){
    return std::bit_cast<int16_t>(static_cast<uint16_t>(clock - sound.deadline)) >= 0;
  }};
  retire_sounds(sound_expired);
  std::erase_if(emitters, expired);
  std::erase_if(trails, expired);
  for(auto &emitter : emitters) advance_emitter(emitter, clock, step);
}

} // namespace darker::game
