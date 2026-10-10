#pragma once

#include <array>
#include <cstdint>
#include <span>
#include "game/city_map.h"
#include "maths/world_coordinates.h"

namespace darker::game {

inline int constexpr beacon_spacing_cells{9};

std::array<uint8_t, 2> beacon_grid_cell(maths::map_position position) noexcept;
std::array<uint8_t, 2> beacon_grid_coordinates(maths::map_position position) noexcept;

uint16_t beacon_light(std::span<city_cell const, city_map_cell_count> cells, maths::world_position position,
  maths::map_fractions fractions);

} // namespace darker::game
