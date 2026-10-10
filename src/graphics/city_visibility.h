#pragma once

#include <cstdint>
#include <limits>
#include <span>
#include <vector>
#include "game/city_map.h"
#include "graphics/camera.h"

namespace darker::graphics {

// Native circular scan arithmetic, separate from map storage and projection precision.
// A wider scanner must deliberately replace byte carry/borrow; increasing the radius alone is insufficient.
struct city_scan_rules {
  using coordinate = uint8_t;
  using error_accumulator = uint8_t;
  static unsigned int constexpr carry_modulus{1u << std::numeric_limits<error_accumulator>::digits};
};

void collect_city_cells(std::span<game::city_cell const, game::city_map_cell_count> cells, city_scan_rules::coordinate column, city_scan_rules::coordinate row,
  camera_angles angles, unsigned int radius, std::vector<uint16_t> &output);

} // namespace darker::graphics
