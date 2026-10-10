#pragma once

#include <array>
#include <cstdint>
#include "game/city_map.h"
#include "maths/world_coordinates.h"

namespace darker::game {

struct radar_coverage {
  static int constexpr region_width_cells{36};
  static int constexpr classification_bias_cells{54};
  static int constexpr grid_alignment_bias_cells{77};
  static unsigned int constexpr mask_row_bits{4};
  static uint16_t constexpr full_coverage_mask{0x777};

  std::array<uint8_t,2> centre{};
  uint16_t mask{0};

  bool contains(uint8_t column, uint8_t row) const noexcept;
};

radar_coverage make_radar_coverage(city_map const &cells, maths::map_position player, bool underground = false);

} // namespace darker::game
