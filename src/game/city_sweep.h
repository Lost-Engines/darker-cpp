#pragma once

#include <array>
#include <cstdint>
#include <span>
#include "game/city_map.h"
#include "maths/world_coordinates.h"
#include "resources/geometry_bank.h"

namespace darker::game {

enum class city_contact { none, terrain, building };

struct city_collision_result {
  city_contact contact{city_contact::none};
  std::uint8_t column{0};
  std::uint8_t row{0};
  std::uint8_t category{0};
};

city_collision_result sweep_city(resources::geometry_bank const &bank, std::span<city_cell const, city_map_cell_count> cells,
  std::uint8_t damage_mask, maths::world_position const &start, maths::world_position &end,
  std::uint16_t expansion = 12, std::int16_t terrain_height = 10);

} // namespace darker::game
