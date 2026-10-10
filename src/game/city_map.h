#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include "maths/world_coordinates.h"

namespace darker::game {

struct city_cell {
  static uint8_t constexpr permitted_target{0x40};                             // objective eligibility; independent of the model's damage/variant bits
  uint8_t type{0};
  uint8_t state{0};                                                             // model-specific packed variants/damage flags, or beacon light level
};

inline maths::map_coordinates<int> constexpr city_map_size{
  .column{128},
  .row{128},
};
inline int constexpr city_map_cell_count{city_map_size.column * city_map_size.row};

using city_map = std::array<city_cell, city_map_cell_count>;

city_map make_city_map(std::span<std::byte const> types, bool energise_beacons);
void assign_city_variants(city_map &cells, std::span<uint8_t const, 256> limits);

} // namespace darker::game
