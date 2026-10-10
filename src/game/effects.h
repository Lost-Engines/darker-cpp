#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "game/effect_sound.h"
#include "game/time.h"
#include "maths/world_coordinates.h"
#include "vectorstorm/vector/vector3.h"

namespace darker::game {

struct emitter_definition {
  game_duration delay{0};
  vec3<int16_t> offset{};
  uint16_t radius{0};
  uint16_t angle{0};
  uint16_t sampling{0};
  int8_t radius_rate{0};
  int8_t height_rate{0};
  int8_t angle_rate{0};
  uint8_t flags{0};
};

struct effect_recipe {
  uint16_t address;
  std::span<emitter_definition const> emitters;
  std::span<effect_sound_definition const> sounds;
};

enum class particle_animation_mode : uint8_t {
  smoke,
  fire
};

class emitter_animation {
public:
  static unsigned int constexpr ticks_per_phase{64};
  static uint8_t constexpr lifetime_mask{0x7f};
  static uint8_t constexpr fire_flag{0x80};

  constexpr emitter_animation() noexcept = default;
  explicit constexpr emitter_animation(uint8_t native) noexcept : value{native} {}

  constexpr unsigned int lifetime_ticks() const noexcept { return (value & lifetime_mask) * ticks_per_phase; }
  constexpr particle_animation_mode mode() const noexcept { return value & fire_flag ? particle_animation_mode::fire : particle_animation_mode::smoke; }
  constexpr uint8_t encoded() const noexcept { return value; }

private:
  uint8_t value{0};
};

using particle_frame = uint8_t;
using damage_severity = uint8_t;

struct particle_emitter {
  maths::world_position position{};
  uint8_t height_fraction{0};
  clock_tick start{0};
  uint16_t radius{0};
  uint16_t angle{0};
  uint16_t sampling{1};
  int8_t radius_rate{0};
  int8_t height_rate{0};
  int8_t angle_rate{0};
  emitter_animation animation{};
};

std::optional<particle_frame> particle_phase(particle_emitter const &emitter, clock_tick clock) noexcept;
void advance_emitter(particle_emitter &emitter, clock_tick clock, game_duration step) noexcept;
std::optional<damage_severity> damage_trail_severity(uint16_t damage, uint8_t flags, uint16_t changes) noexcept;
particle_emitter make_damage_trail(maths::world_position position, damage_severity severity, uint16_t random, clock_tick clock) noexcept;

class effect_system {
public:
  static unsigned int constexpr trail_capacity{20};                            // shared pool for damage trails and stationary impact sparks
  static unsigned int constexpr emitter_capacity{25};                          // separate pool for moving recipe emitters

private:
  effect_sound_pool effect_sounds{1};
  effect_sound_pool impact_sounds{1 + effect_sound_pool::capacity};

public:
  std::vector<particle_emitter> emitters;
  std::vector<particle_emitter> trails;
  std::span<effect_sound const> sounds() const noexcept { return effect_sounds.records(); }
  std::span<effect_sound const> gun_sounds() const noexcept { return impact_sounds.records(); }

  template<typename Visitor> void submit_sounds(Visitor const &visit) {
    /// Keep recipe sounds before impact sounds in native source-admission order
    effect_sounds.submit(visit);
    impact_sounds.submit(visit);
  }

  template<typename Predicate> void retire_sounds(Predicate const &expired) {
    /// Retire sources through their owning pools so active records and free identities remain consistent
    effect_sounds.retire(expired);
    impact_sounds.retire(expired);
  }

  void spark(maths::world_position position, uint8_t phase, uint16_t sound_level, clock_tick clock);
  void gun_impact(maths::world_position position, bool hit, clock_tick clock);
  void spawn(uint16_t recipe, maths::world_position position, clock_tick clock);
  void trail(maths::world_position position, damage_severity severity, uint16_t &random, clock_tick clock);
  void advance(clock_tick clock, game_duration step);
};

} // namespace darker::game
