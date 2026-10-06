#pragma once

#include <cstdint>
#include <vector>

namespace darker::game {

struct collision_cell {
  std::uint8_t column{0};
  std::uint8_t row{0};
};

std::vector<collision_cell> swept_collision_cells(std::uint16_t x, std::uint16_t y, std::uint16_t end_x, std::uint16_t end_y);

} // namespace darker::game
