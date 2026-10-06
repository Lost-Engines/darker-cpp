#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace darker::game {

struct city_cell {
  std::uint8_t type{0};
  std::uint8_t state{0};
};

std::uint16_t beacon_light(std::span<city_cell const, 128 * 128> cells, std::array<std::uint16_t, 3> position,
  std::array<std::uint8_t, 2> fractions);

} // namespace darker::game
