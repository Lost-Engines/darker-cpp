#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace darker::game {

struct emitter_definition {
  uint16_t delay{0};
  std::array<int16_t, 3> offset{};
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
};

struct particle_emitter {
  std::array<uint16_t, 3> position{};
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
particle_emitter make_damage_trail(std::array<uint16_t, 3> position, uint8_t severity, uint16_t random, uint16_t clock) noexcept;

class effect_system {
public:
  std::vector<particle_emitter> emitters;
  std::vector<particle_emitter> trails;

  void spawn(uint16_t recipe, std::array<uint16_t, 3> position, uint16_t clock);
  void trail(std::array<uint16_t, 3> position, uint8_t severity, uint16_t &random, uint16_t clock);
  void advance(uint16_t clock, uint16_t step);
};

} // namespace darker::game
