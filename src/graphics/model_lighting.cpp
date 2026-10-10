#include "graphics/model_lighting.h"
#include <algorithm>
#include <stdexcept>

namespace darker::graphics {

distance_shading::distance_shading(unsigned int const count) {
  /// B73C builds palette-index ramps towards shade one, with a half-step initial fraction and signed IDIV rounding
  unsigned int constexpr minimum_distance_tables{2};                           // interpolation needs both a near and a far endpoint
  int constexpr interpolated_shades{model_colours::shade_count};               // ramp entries 28–31 have special renderer meanings
  int constexpr distant_shade{1};
  int constexpr shade_fraction_scale{256};
  int constexpr half_shade{shade_fraction_scale / 2};
  if(count < minimum_distance_tables || count > scene_limits::maximum_distance_shade_tables) throw std::invalid_argument{"Distance shade table count exceeds its original byte range"};
  tables.resize(count);
  for(int shade{0}; shade < interpolated_shades; ++shade) {
    int const step{((shade - distant_shade) * shade_fraction_scale) / static_cast<int>(count - 1)};
    for(unsigned int distance{0}; distance < count; ++distance) {
      tables[distance][shade] = static_cast<uint8_t>((shade * shade_fraction_scale + half_shade - static_cast<int>(distance) * step) >> 8);
    }
  }
}

model_colours distance_shading::colours(render_geometry::coordinate_bits depth, model_path const path, uint8_t const light) const {
  /// 2CE4 folds beacon strength into distance; the near path clamps a negative origin depth before the wrapping subtraction
  render_geometry::coordinate_bits constexpr depth_sign_bit{render_geometry::coordinate_bits{1} << (render_geometry::whole_bits - 1)};
  unsigned int constexpr full_beacon_light{255};
  unsigned int constexpr darkness_depth_scale{16};                             // each missing light unit adds 16 projection-depth units
  unsigned int constexpr dynamic_colour_base{0xf0};                            // palette ramp 7, upper half; light supplies the low nibble
  // depth wraps before selecting its high byte, preserving the original distance-table lookup
  if(path == model_path::near_clipped && (depth & depth_sign_bit)) depth = 0;
  auto const adjusted{static_cast<render_geometry::coordinate_bits>(depth + darkness_depth_scale * (full_beacon_light - light))};
  auto const index{std::min<size_t>(adjusted >> scene_limits::distance_shade_shift, tables.size() - 1)};
  return {
    .shades{tables[index]},
    .dynamic{static_cast<uint8_t>(dynamic_colour_base | (light >> 4))}
  };
}

} // namespace darker::graphics
