#include "graphics/model_projection.h"
#include <bit>

namespace darker::graphics {
namespace {

projection_term product(int16_t const value, int16_t const coefficient) noexcept {
  /// FD04/FD30/FD5C cache the upper word and middle byte of each signed product
  auto const bits{static_cast<render_geometry::accumulator_bits>(static_cast<render_geometry::accumulator>(value) * coefficient)};
  return {
    .whole{static_cast<render_geometry::coordinate_bits>(bits >> render_geometry::product_whole_shift)},
    .fraction{static_cast<render_geometry::fraction>(bits >> render_geometry::product_fraction_shift)}
  };
}

void negate(projection_term &term) noexcept {
  /// Negate the cached 24-bit contribution rather than recomputing an unrounded product
  term.whole = static_cast<render_geometry::coordinate_bits>(-term.whole - (term.fraction != 0 ? 1 : 0));
  term.fraction = static_cast<render_geometry::fraction>(-term.fraction);
}

render_geometry::accumulator_bits bits(projection_term const term) noexcept {
  /// Join the original coordinate word and fractional byte
  return static_cast<render_geometry::accumulator_bits>(term.whole) * render_geometry::fraction_scale + term.fraction;
}

} // anonymous namespace

model_projection::model_projection(projection_parameters const supplied) : parameters{supplied} {
  /// Begin with zero cached contributions; coordinate bytecode supplies each subsequent update
}

void model_projection::set_component(size_t const axis, int16_t const value) {
  /// Replace one transformed component, retaining the native shared A/B vertical fraction
  auto &cached{contributions.at(axis)};
  auto const coefficients{parameters.axes.at(axis)};
  cached.horizontal = product(value, coefficients.horizontal);
  cached.depth = product(value, coefficients.depth);
  auto const vertical{product(value, coefficients.vertical)};
  cached.vertical = vertical.whole;
  (axis == 2 ? vertical_c_fraction : vertical_ab_fraction) = vertical.fraction;
}

void model_projection::zero_component(size_t const axis) {
  /// FF05/FF1F/FF3D clear a contribution and its associated shared fractional byte
  contributions.at(axis) = {};
  (axis == 2 ? vertical_c_fraction : vertical_ab_fraction) = 0;
}

void model_projection::negate_component(size_t const axis) {
  /// FF59/FF82/FFB0 retain the rounding already present in the cached products
  auto &cached{contributions.at(axis)};
  negate(cached.horizontal);
  negate(cached.depth);
  auto &fraction{axis == 2 ? vertical_c_fraction : vertical_ab_fraction};
  cached.vertical = static_cast<render_geometry::coordinate_bits>(-cached.vertical - (fraction != 0 ? 1 : 0));
  fraction = static_cast<render_geometry::fraction>(-fraction);
}

camera_vertex model_projection::transform() const noexcept {
  /// FC97 accumulates cached words and fractional carries before the two signed divisions
  auto horizontal{bits(parameters.horizontal)};
  auto depth{bits(parameters.depth)};
  auto vertical{bits(parameters.vertical) + vertical_ab_fraction + vertical_c_fraction};
  for(auto const &cached : contributions) {
    horizontal += bits(cached.horizontal);
    depth += bits(cached.depth);
    vertical += static_cast<render_geometry::accumulator_bits>(cached.vertical) * render_geometry::fraction_scale;
  }
  return {
    .horizontal{render_geometry::wrap_projection(horizontal)},
    .vertical{render_geometry::wrap_projection(vertical)},
    .depth{render_geometry::wrap_projection(depth)}
  };
}

projected_vertex model_projection::project() const {
  /// The direct path divides the same cached camera coordinates that the near path retains
  auto const vertex{transform()};
  return {
    .screen{project_vertex(vertex, parameters.origin)},
    .depth{render_geometry::wrap_coordinate(vertex.depth >> render_geometry::fraction_bits)}
  };
}

} // namespace darker::graphics
