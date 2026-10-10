#pragma once

#include <array>
#include <cstdint>
#include <span>
#include "game/city_map.h"
#include "game/collision_box.h"
#include "maths/world_coordinates.h"
#include "resources/geometry_bank.h"

namespace darker::game {

enum class city_contact { none, terrain, building };

struct city_collision_result {
  city_contact contact{city_contact::none};
  uint8_t column{0};
  uint8_t row{0};
  collision_category category{collision_category::ground_target};
};

city_collision_result sweep_city(resources::geometry_bank const &bank, std::span<city_cell const, city_map_cell_count> cells,
  uint8_t damage_mask, maths::world_position const &start, maths::world_position &end,
  uint16_t expansion = 12, int16_t terrain_height = 10);

} // namespace darker::game
