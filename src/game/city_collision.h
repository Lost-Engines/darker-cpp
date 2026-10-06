#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include "resources/geometry_bank.h"

namespace darker::game {

struct collision_box {
  std::array<std::uint16_t, 3> minimum{};                                    // column, row and height in the collision routine's units
  std::array<std::uint16_t, 3> maximum{};
  std::uint8_t category{0};
};

std::vector<collision_box> city_collision_boxes(resources::geometry_bank const &bank, unsigned int type,
  std::uint8_t state, std::uint8_t damage_mask, std::uint8_t column, std::uint8_t row, std::uint16_t expansion = 0);

} // namespace darker::game
