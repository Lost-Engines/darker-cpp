#include "game/effects.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include "game/effect_tables.h"
#include "game/random.h"

namespace darker::game {

std::optional<uint8_t> particle_phase(particle_emitter const &emitter, uint16_t const clock) noexcept {
  /// 6C01/6C65 age backwards, retaining phase zero at the exact final tick
  auto const elapsed{std::bit_cast<int16_t>(static_cast<uint16_t>(clock - emitter.start))};
  if(elapsed < 0) return std::nullopt;
  int const remaining{(emitter.flags & 127) * 64 - elapsed};
  if(remaining < 0) return std::nullopt;
  auto phase{static_cast<uint8_t>(remaining >> 6)};
  if((emitter.flags & 128) && phase >= 10) phase += 6;
  return phase;
}

void advance_emitter(particle_emitter &emitter, uint16_t const clock, uint16_t const step) noexcept {
  /// 6CD8 preserves signed byte rates and carries the fractional height into the wrapping word
  if(std::bit_cast<int16_t>(static_cast<uint16_t>(clock - emitter.start)) < 0) return;
  auto const ticks{std::bit_cast<int8_t>(static_cast<uint8_t>(step))};
  int const delta{emitter.height_rate * ticks};
  unsigned int const fraction{static_cast<unsigned int>(emitter.height_fraction) + static_cast<uint8_t>(delta * 32)};
  emitter.height_fraction = static_cast<uint8_t>(fraction);
  emitter.position[2] = static_cast<uint16_t>(emitter.position[2] + (delta >> 3) + (fraction >> 8));
  emitter.radius = static_cast<uint16_t>(emitter.radius + emitter.radius_rate * ticks);
  emitter.angle = static_cast<uint16_t>(emitter.angle + emitter.angle_rate * ticks);
}

std::optional<uint8_t> damage_trail_severity(uint16_t const damage, uint8_t const flags, uint16_t const changes) noexcept {
  /// 79C2/7A39 gate trails using damage high-byte bands and accumulated timer transitions
  if(flags & 0x18) return std::nullopt;
  auto const level{damage >> 8};
  if(level < 128) return std::nullopt;
  auto const mask{level < 160 ? 0x400 >> ((level - 128) >> 3) : 0x40};
  if(!(mask & changes)) return std::nullopt;
  return static_cast<uint8_t>(level < 160 ? 0 : std::min(level - 160, 63));
}

particle_emitter make_damage_trail(std::array<uint16_t, 3> position, uint8_t const severity, uint16_t const random, uint16_t const clock) noexcept {
  /// 676E jitters the saved position using one original random word, then leaves the sprite stationary
  position[1] = static_cast<uint16_t>(position[1] + (std::bit_cast<int16_t>(random) >> 11));
  auto const shifted{static_cast<uint16_t>(random << 5)};
  auto const dx{std::bit_cast<int16_t>(shifted) >> 11};
  position[0] = static_cast<uint16_t>(position[0] + dx);
  position[2] = static_cast<uint16_t>(position[2] + std::bit_cast<int8_t>(static_cast<uint8_t>(random * 4)));
  return {.position{position}, .start{clock}, .flags{static_cast<uint8_t>(0x88 + (severity >> 3))}};
}

void effect_system::spawn(uint16_t const recipe, std::array<uint16_t, 3> const position, uint16_t const clock) {
  /// 6820 expands the executable recipe into independently delayed, moving emitter records
  auto const found{std::ranges::find(original_effect_recipes, recipe, &effect_recipe::address)};
  if(found == original_effect_recipes.end()) throw std::invalid_argument{"Unknown effect recipe"};
  for(auto const &source : found->emitters) {
    particle_emitter emitter{
      .position{position}, .start{static_cast<uint16_t>(clock + source.delay)}, .radius{source.radius}, .angle{source.angle},
      .sampling{source.sampling}, .radius_rate{source.radius_rate}, .height_rate{source.height_rate},
      .angle_rate{source.angle_rate}, .flags{source.flags},
    };
    for(size_t axis{0}; axis < 3; ++axis) emitter.position[axis] = static_cast<uint16_t>(emitter.position[axis] + source.offset[axis]);
    if(emitters.size() == 25) emitters.erase(emitters.begin());
    emitters.push_back(emitter);
  }
  for(auto const &sound : found->sounds) {
    if(sounds.size() == 16) sounds.erase(sounds.begin());
    sounds.push_back({.position{position}, .definition{sound}, .deadline{static_cast<uint16_t>(clock + sound.duration)}, .identity{next_sound_identity++}});
  }
}

void effect_system::gun_impact(std::array<uint16_t, 3> position, bool const hit, uint16_t const clock) {
  /// 6730/6742 create a short endpoint sprite and an independently timed patch-22 sound
  position[2] &= 0xfff8;
  if(trails.size() == 20) trails.erase(trails.begin());
  trails.push_back({.position{position}, .start{clock}, .flags{static_cast<uint8_t>(hit ? 3 : 6)}});
  if(gun_sounds.size() == 16) gun_sounds.erase(gun_sounds.begin());
  gun_sounds.push_back({.position{position}, .definition{.duration{256}, .pitch{0x203},
    .level{static_cast<uint16_t>(hit ? 0xde30 : 0xce30)}, .patch{22}, .flags{1}},
    .deadline{static_cast<uint16_t>(clock + 256)}, .identity{next_sound_identity++}});
}

void effect_system::trail(std::array<uint16_t, 3> const position, uint8_t const severity, uint16_t &random, uint16_t const clock) {
  /// Advance the shared random sequence only when a trail is emitted
  if(trails.size() == 20) trails.erase(trails.begin());
  trails.push_back(make_damage_trail(position, severity, next_random(random), clock));
}

void effect_system::advance(uint16_t const clock, uint16_t const step) {
  /// Remove expired effects before movement; delayed records retain their initial state
  auto const expired{[&](auto const &emitter){
    return std::bit_cast<int16_t>(static_cast<uint16_t>(clock - emitter.start)) > (emitter.flags & 127) * 64;
  }};
  auto const sound_expired{[&](auto const &sound){ return std::bit_cast<int16_t>(static_cast<uint16_t>(clock - sound.deadline)) >= 0; }};
  std::erase_if(sounds, sound_expired);
  std::erase_if(gun_sounds, sound_expired);
  std::erase_if(emitters, expired);
  std::erase_if(trails, expired);
  for(auto &emitter : emitters) advance_emitter(emitter, clock, step);
}

} // namespace darker::game
