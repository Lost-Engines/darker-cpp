#pragma once

#include <cstdint>
#include "maths/world_coordinates.h"

namespace darker::game {

struct collision_sweep_format {
  using coordinate = maths::world_format::position_component;
  static unsigned int constexpr cell_fraction_bits{maths::world_format::cell_fraction_bits};
  static unsigned int constexpr cell_fraction_mask{(1u << cell_fraction_bits) - 1};
  static unsigned int constexpr slope_fraction_bits{16};
  static unsigned int constexpr error_mask{(1u << slope_fraction_bits) - 1};
  static unsigned int constexpr crossing_bias{128};                           // native fractional crossing bias, not a collision radius
};

// collision-stream encoding is independent of the runtime sweep arithmetic
struct collision_stream_format {
  static unsigned int constexpr world_data_base{0x8000};
  static uint8_t constexpr first_terminator{0xf8};                             // f8–ff end the stream
  static uint8_t constexpr first_height_prefix{0xf0};                          // f0–f7 precede a box with an 11-bit lower-height offset
  static uint8_t constexpr height_high_bits{7};
  static unsigned int constexpr category_shift{3};
  static int constexpr height_units_per_marker{32};
  static unsigned int constexpr horizontal_endpoint_bytes{4};
};

} // namespace darker::game
