#pragma once

#include <array>
#include <cstdint>
#include "game/city_map.h"
#include "maths/world_coordinates.h"

namespace darker::game {

struct radar_coverage {
  std::array<uint8_t,2> centre{};
  uint16_t mask{0};

  bool contains(uint8_t column, uint8_t row) const noexcept;
};

radar_coverage make_radar_coverage(city_map const &cells, maths::map_position player, bool underground = false);

} // namespace darker::game
