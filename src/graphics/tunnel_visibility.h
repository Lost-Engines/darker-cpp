#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include "game/city_map.h"

namespace darker::graphics {

void visit_tunnel_cells(std::span<game::city_cell const, game::city_map_cell_count> cells, uint8_t column, uint8_t row,
  std::span<uint8_t, game::city_map_cell_count> visibility, std::function<bool(uint16_t)> const &visit);

} // namespace darker::graphics
