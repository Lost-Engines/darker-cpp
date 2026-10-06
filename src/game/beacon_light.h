#pragma once

#include <array>
#include <cstdint>
#include <span>
#include "game/city_map.h"

namespace darker::game {

std::array<uint8_t, 2> beacon_grid_coordinates(std::array<uint16_t, 2> position) noexcept;

std::uint16_t beacon_light(std::span<city_cell const, 128 * 128> cells, std::array<std::uint16_t, 3> position,
  std::array<std::uint8_t, 2> fractions);

} // namespace darker::game
