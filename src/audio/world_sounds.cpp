#include "audio/world_sounds.h"
#include <algorithm>
#include <bit>
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

fm_frame world_sounds::mix(fm_frame const &player, game::mission_combat const &combat, game::object_pose const &listener) {
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
  for(auto const *pool : {&combat.projectiles,&combat.hostile_projectiles}) {
    for(auto *shot{pool->objects().head}; shot; shot = shot->next) {
      if(shot->flags & 8) continue;
      auto const &definition{*shot->parameters.definition};
      auto level{static_cast<uint16_t>(definition.sound_level * 256 + 255)};
      if(shot->flags & 0x20) level = static_cast<uint16_t>((static_cast<uint32_t>(level) * (shot->fade * 257)) >> 16);
      game::effect_sound const sound{.position{shot->placement.position}, .definition{
        .duration{0}, .pitch{&definition == &game::original_object_definitions[6] ? combat.dual_launch_pitch : definition.sound_pitch}, .level{level}, .patch{definition.fm_patch}, .flags{0}}};
      // Native IDs distinguish the two fixed pools; expiry distinguishes successive launches in a reused slot.
      auto const identity{0x200000000ULL + static_cast<uint64_t>(shot->deadline) * 65536 + static_cast<uint64_t>(shot->native_id)};
      append(sound, &shot->placement, identity);
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
