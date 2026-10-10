#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "vectorstorm/vector/vector3.h"
#include "game/effect_sound.h"
#include "maths/world_coordinates.h"

namespace darker::game {

struct emitter_definition {
  uint16_t delay{0};
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

struct particle_emitter {
  maths::world_position position{};
  uint8_t height_fraction{0};
  uint16_t start{0};
  uint16_t radius{0};
  uint16_t angle{0};
  uint16_t sampling{1};
  int8_t radius_rate{0};
  int8_t height_rate{0};
  int8_t angle_rate{0};
  uint8_t flags{0};
};

std::optional<uint8_t> particle_phase(particle_emitter const &emitter, uint16_t clock) noexcept;
void advance_emitter(particle_emitter &emitter, uint16_t clock, uint16_t step) noexcept;
std::optional<uint8_t> damage_trail_severity(uint16_t damage, uint8_t flags, uint16_t changes) noexcept;
particle_emitter make_damage_trail(maths::world_position position, uint8_t severity, uint16_t random, uint16_t clock) noexcept;

class effect_system {
private:
  struct sound_slots {
    std::array<uint8_t, 16> free{15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    std::array<uint16_t, 16> generations{};
    size_t count{16};
  };
  sound_slots effect_slots, gun_slots;
  void append_sound(std::vector<effect_sound> &pool, sound_slots &slots, uint32_t first_identity, effect_sound sound);

public:
  std::vector<particle_emitter> emitters;
  std::vector<particle_emitter> trails;
  std::vector<effect_sound> sounds;
  std::vector<effect_sound> gun_sounds;

  template<typename Predicate> void retire_sounds(Predicate const &expired) {
    /// Native 1CD4 prepends retired records to the free list in newest-first traversal order
    auto const retire{[&](auto &pool, auto &slots, uint32_t const first_identity){
      for(size_t i{pool.size()}; i != 0; --i) {
        auto const index{i - 1};
        if(!expired(pool[index])) continue;
        slots.free[slots.count++] = static_cast<uint8_t>(pool[index].identity - first_identity);
        pool.erase(pool.begin() + static_cast<ptrdiff_t>(index));
      }
    }};
    retire(sounds, effect_slots, 1);
    retire(gun_sounds, gun_slots, 17);
  }

  void spark(maths::world_position position, uint8_t phase, uint16_t sound_level, uint16_t clock);
  void gun_impact(maths::world_position position, bool hit, uint16_t clock);
  void spawn(uint16_t recipe, maths::world_position position, uint16_t clock);
  void trail(maths::world_position position, uint8_t severity, uint16_t &random, uint16_t clock);
  void advance(uint16_t clock, uint16_t step);
};

} // namespace darker::game
