#pragma once

#include <cstdint>
#include <vector>

namespace darker::game {

struct collision_cell {
  uint8_t column{0};
  uint8_t row{0};
};

std::vector<collision_cell> swept_collision_cells(uint16_t x, uint16_t y, uint16_t end_x, uint16_t end_y);

} // namespace darker::game
