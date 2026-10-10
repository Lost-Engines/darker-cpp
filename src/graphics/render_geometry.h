#pragma once

#include <bit>
#include <cstdint>
#include <limits>
#include <type_traits>
#include "maths/world_coordinates.h"
#include "graphics/screen_layout.h"

namespace darker::graphics {

// runtime rendering arithmetic, deliberately separate from model-file words and gameplay positions
struct render_geometry {
  using coordinate = int16_t;
  using coordinate_bits = std::make_unsigned_t<coordinate>;
  using fraction = uint8_t;
  using accumulator = int32_t;
  using accumulator_bits = std::make_unsigned_t<accumulator>;
  using screen_coordinate = coordinate;
  using sorting_distance = coordinate_bits;

  static unsigned int constexpr whole_bits{std::numeric_limits<coordinate_bits>::digits};
  static unsigned int constexpr fraction_bits{std::numeric_limits<fraction>::digits};
  static unsigned int constexpr accumulator_width{std::numeric_limits<accumulator_bits>::digits};
  static unsigned int constexpr product_whole_shift{16};                     // native transform product scale, independent of coordinate storage width
  static unsigned int constexpr product_fraction_shift{product_whole_shift - fraction_bits};
  static unsigned int constexpr retained_bits{whole_bits + fraction_bits};
  static unsigned int constexpr discarded_bits{accumulator_width - retained_bits};
  static accumulator constexpr fraction_scale{accumulator{1} << fraction_bits};
  static accumulator constexpr fraction_mask{fraction_scale - 1};
  static accumulator constexpr focal_length{256};                              // pixels; independent of the fixed-point fraction scale despite the equal native value
  static int constexpr world_to_camera_scale{4};
  static int constexpr model_horizontal_scale{2};
  static int constexpr camera_units_per_cell{maths::world_format::units_per_cell * world_to_camera_scale};
  static unsigned int constexpr camera_fraction_shift{maths::world_format::fraction_bits - 2};
  static coordinate constexpr near_depth{32};
  static accumulator constexpr near_depth_fixed{near_depth * fraction_scale};
  static accumulator constexpr half_near_depth_fixed{near_depth_fixed / 2};
  static unsigned int constexpr near_projection_shift{5};                      // native intersection projection divides by near_depth = 2^5
  static int constexpr intersection_high_word_limit{15};
  static screen_coordinate constexpr intersection_min{-16383};
  static screen_coordinate constexpr intersection_max{16382};

  static_assert(retained_bits < accumulator_width);
  static_assert(focal_length == fraction_scale);                              // native IDIV consumes the fixed-point numerator directly
  static_assert(product_whole_shift >= fraction_bits);
  static_assert((accumulator{1} << near_projection_shift) == near_depth);
  static_assert((1 << (maths::world_format::fraction_bits - camera_fraction_shift)) == world_to_camera_scale);

  static constexpr coordinate wrap_coordinate(accumulator value) noexcept {
    /// Reinterpret the native low coordinate word as signed
    return std::bit_cast<coordinate>(static_cast<coordinate_bits>(value));
  }

  static constexpr accumulator wrap_projection(accumulator_bits value) noexcept {
    /// Sign-extend the retained whole coordinate and fractional byte
    return std::bit_cast<accumulator>(value << discarded_bits) >> discarded_bits;
  }
};

struct scene_limits {
  static unsigned int constexpr surface_radius_cells{15};
  static unsigned int constexpr tunnel_radius_cells{8};
  static unsigned int constexpr minimum_scan_radius_cells{2};
  static unsigned int constexpr maximum_scan_radius_cells{32};                  // scanner bound, not a guarantee that projection can represent the entire radius
  static unsigned int constexpr distance_shade_tables{60};
  static unsigned int constexpr maximum_distance_shade_tables{255};
  static unsigned int constexpr distance_shade_shift{8};                       // whole depth units per fog-table entry; independent of projection fractions
};

} // namespace darker::graphics
