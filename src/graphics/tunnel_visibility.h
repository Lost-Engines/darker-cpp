#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include "game/city_map.h"

namespace darker::graphics {

void visit_tunnel_cells(std::span<game::city_cell const,128*128> cells, uint8_t column, uint8_t row,
  std::span<uint8_t,128*128> visibility, std::function<bool(uint16_t)> const &visit);

} // namespace darker::graphics
