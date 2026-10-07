#include "audio/world_sounds.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include <vector>
#include "game/object_definitions.h"
#include "maths/direction.h"
#include "maths/sine_table.h"

namespace darker::audio {

std::optional<uint16_t> audible_level(std::array<uint16_t, 3> const source, std::array<uint16_t, 3> const listener,
  uint16_t const level, uint8_t const flags) noexcept {
  /// 3488 rejects distant sources before subtracting the original squared-distance attenuation
  if(flags & 2) return level;
  std::array<int32_t, 3> delta{};
  for(size_t axis{0}; axis < 3; ++axis) delta[axis] = std::bit_cast<int16_t>(static_cast<uint16_t>(source[axis] - listener[axis]));
  auto const x{static_cast<uint32_t>(delta[0] * delta[0])};
  auto const y{static_cast<uint32_t>(delta[1] * delta[1])};
  if(x >= 0x1000000 || y >= 0x1000000) return std::nullopt;
  auto const z{static_cast<uint64_t>(delta[2] * delta[2]) * 4};
  if(z >= 0x100000000ULL) return std::nullopt;
  auto const distance{(x >> 8) + (y >> 8) + (z >> 16)};
  if(distance >= 0x4000 || distance * 4 >= level) return std::nullopt;
  return static_cast<uint16_t>(level - distance * 4);
}

uint16_t doppler_factor(game::object_pose const *const motion, uint16_t const heading, uint16_t const pitch) noexcept {
  /// 3ACD projects wrapping speed onto the source bearing using two cosine table products
  if(!motion) return 0x39d0;
  auto const cosine{[](uint16_t const angle){ return maths::original_sine[((angle >> 6) + 256) % 1024]; }};
  auto const product{(cosine(static_cast<uint16_t>(motion->angles[1] - pitch)) * cosine(static_cast<uint16_t>(motion->angles[0] - heading))) >> 16};
  auto const speed{std::bit_cast<int16_t>(static_cast<uint16_t>(motion->speed * 4))};
  return static_cast<uint16_t>(0x39d0 - ((speed * product) >> 16));
}

uint16_t spatial_pitch(uint16_t const pitch, std::array<uint16_t, 3> const source, game::object_pose const &listener,
  game::object_pose const *const source_motion) noexcept {
  /// 39BA scales both horizontal differences by eight before 925C, then divides listener/source velocity factors
  auto const x{static_cast<uint16_t>((source[0] - listener.position[0]) * 8)};
  auto const y{static_cast<uint16_t>((source[1] - listener.position[1]) * 8)};
  auto const z{static_cast<uint16_t>(source[2] - listener.position[2])};
  auto const magnitude{[](uint16_t const value){ return value & 0x8000 ? static_cast<uint16_t>(-value) : value; }};
  auto const heading{static_cast<uint16_t>(maths::direction_index(x, y) << 5)};
  auto const elevation{static_cast<uint16_t>(maths::direction_index(z, std::max(magnitude(x), magnitude(y))) << 5)};
  return static_cast<uint16_t>((static_cast<uint32_t>(doppler_factor(&listener, heading, elevation)) * pitch)
    / doppler_factor(source_motion, heading, elevation));
}

fm_note object_sound(game::object_definition const &definition, game::object_pose const &pose,
  object_sound_state const state, uint16_t const clock) {
  /// 3556 supplies each non-player definition's callback with its original pitch, level and object state
  fm_note result{.pitch{definition.sound_pitch},.level{static_cast<uint16_t>(definition.sound_level*256+255)},.patch{definition.fm_patch}};
  switch(definition.sound_entry) {
  case 0x391d: {
    auto const phase{static_cast<uint8_t>(clock+state.identity)};
    auto const triangle{static_cast<uint8_t>(phase ^ (phase & 128 ? 255 : 0))};
    auto const amplitude{std::min(255u,64u+(state.damage >> 8))};
    result.pitch = static_cast<uint16_t>(result.pitch+(pose.speed >> 2)+((triangle*amplitude) >> 10));
    [[fallthrough]];
  }
  case 0x393d: result.active = !(state.flags & 0x28); break;
  case 0x3942:
    result.pitch = static_cast<uint16_t>(result.pitch-(std::bit_cast<int16_t>(pose.angles[1]) >> 4));
    if(pose.speed == 0) result.pitch >>= 1;
    result.active = !(state.flags & 8);
    break;
  case 0x3957: {
    auto const remaining{static_cast<uint16_t>(state.deadline-clock)};
    if(std::bit_cast<int8_t>(static_cast<uint8_t>(remaining >> 8)) > 0) {
      result.pitch = static_cast<uint16_t>(result.pitch+(remaining >> 4));
      result.active = !(state.flags & 8);
      break;
    }
    [[fallthrough]];
  }
  case 0x3969:
    if(state.flags & 0x20) {
      result.level = static_cast<uint16_t>((uint32_t{result.level}*(state.fade*257u)) >> 16);
      result.active = result.level != 0;
    } else result.active = !(state.flags & 8);
    break;
  default: throw std::invalid_argument{"Object sound requires a non-player sound callback"};
  }
  return result;
}

fm_frame world_sounds::mix(fm_frame const &player, game::mission_combat const &combat, game::object_pose const &listener, uint16_t const clock) {
  /// Admit spatial effects and preserve assigned channels while selecting the nine strongest active sources
  struct candidate { uint64_t identity; fm_note note; };
  std::vector<candidate> candidates;
  for(size_t i{0}; i < player.size(); ++i) {
    if(player[i].active) candidates.push_back({0x100000000ULL + i * 65536 + player[i].generation, player[i]});
  }
  auto const append{[&](game::effect_sound const &sound, game::object_pose const *const motion, uint64_t const identity){
    auto const &definition{sound.definition};
    auto const level{audible_level(sound.position, listener.position, definition.level, definition.flags)};
    if(!level) return;
    auto const pitch{definition.flags & 4 ? definition.pitch : spatial_pitch(definition.pitch, sound.position, listener, motion)};
    candidates.push_back({identity, {.pitch{pitch}, .level{*level}, .patch{definition.patch}, .active{true}}});
  }};
  for(auto i{combat.effects.sounds.rbegin()}; i != combat.effects.sounds.rend(); ++i) append(*i, nullptr, i->identity);
  for(auto i{combat.effects.gun_sounds.rbegin()}; i != combat.effects.gun_sounds.rend(); ++i) append(*i, nullptr, i->identity);
  for(auto const &actor : combat.actors) {
    if(!actor.parameters.definition) continue;
    auto const note{object_sound(*actor.parameters.definition,actor.pose,
      {.identity{static_cast<uint16_t>(0xd986+actor.index*112)},.flags{actor.flags},.damage{actor.awareness.cooldown},
        .fade{actor.fade},.deadline{actor.expiry}},clock)};
    if(!note.active) continue;
    game::effect_sound const sound{.position{actor.pose.position},.definition{
      .duration{0},.pitch{note.pitch},.level{note.level},.patch{note.patch},.flags{0}}};
    append(sound,&actor.pose,0x300000000ULL+actor.index);
  }
  for(auto const *pool : {&combat.projectiles,&combat.hostile_projectiles}) {
    for(auto *shot{pool->objects().head}; shot; shot = shot->next) {
      auto const &definition{*shot->parameters.definition};
      auto note{object_sound(definition,shot->placement,
        {.identity{shot->native_id},.flags{shot->flags},.fade{shot->fade},.deadline{shot->deadline}},clock)};
      if(!note.active) continue;
      if(&definition == &game::original_object_definitions[6]) note.pitch = combat.dual_launch_pitch;
      game::effect_sound const sound{.position{shot->placement.position},.definition{
        .duration{0},.pitch{note.pitch},.level{note.level},.patch{note.patch},.flags{0}}};
      // Native IDs distinguish the two fixed pools; expiry distinguishes successive launches in a reused slot.
      auto const identity{0x200000000ULL+static_cast<uint64_t>(shot->deadline)*65536+shot->native_id};
      append(sound,&shot->placement,identity);
    }
  }
  std::stable_sort(candidates.begin(), candidates.end(), [](auto const &a, auto const &b){ return a.note.level > b.note.level; });
  if(candidates.size() > 9) candidates.resize(9);
  fm_frame frame{};
  std::array<bool, 9> assigned{};
  for(auto const &candidate : candidates) {
    auto found{std::ranges::find(owners, candidate.identity)};
    if(found == owners.end()) continue;
    size_t const channel{static_cast<size_t>(found - owners.begin())};
    frame[channel] = candidate.note;
    frame[channel].generation = generations[channel];
    assigned[channel] = true;
  }
  for(auto const &candidate : candidates) {
    if(std::ranges::find(owners, candidate.identity) != owners.end()) continue;
    auto const free{std::ranges::find(assigned, false)};
    size_t const channel{static_cast<size_t>(free - assigned.begin())};
    owners[channel] = candidate.identity;
    frame[channel] = candidate.note;
    frame[channel].generation = ++generations[channel];
    assigned[channel] = true;
  }
  for(size_t i{0}; i < owners.size(); ++i) if(!assigned[i]) owners[i] = 0;
  return frame;
}

} // namespace darker::audio
