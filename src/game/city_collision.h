#pragma once

#include <cstdint>
#include <vector>
#include "game/collision_box.h"
#include "resources/geometry_bank.h"

namespace darker::game {

std::vector<collision_box> city_collision_boxes(resources::geometry_bank const &bank, unsigned int type,
  std::uint8_t state, std::uint8_t damage_mask, std::uint8_t column, std::uint8_t row, std::uint16_t expansion = 0);

} // namespace darker::game
