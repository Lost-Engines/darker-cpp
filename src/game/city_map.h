#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace darker::game {

struct city_cell {
  std::uint8_t type{0};
  std::uint8_t state{0};
};

using city_map = std::array<city_cell, 128 * 128>;

city_map make_city_map(std::span<std::byte const> types, bool energise_beacons);
void assign_city_variants(city_map &cells, std::span<std::uint8_t const, 256> limits);

} // namespace darker::game
