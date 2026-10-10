#include "graphics/model_projection.h"
#include <bit>

namespace darker::graphics {
namespace {

projection_term product(std::int16_t const value, std::int16_t const coefficient) noexcept {
  /// FD04/FD30/FD5C cache the upper word and middle byte of each signed product
  auto const bits{static_cast<std::uint32_t>(static_cast<std::int32_t>(value) * coefficient)};
  return {
    .whole{static_cast<std::uint16_t>(bits >> 16)},
    .fraction{static_cast<std::uint8_t>(bits >> 8)}
  };
}

void negate(projection_term &term) noexcept {
  /// Negate the cached 24-bit contribution rather than recomputing an unrounded product
  term.whole = static_cast<std::uint16_t>(-term.whole - (term.fraction != 0 ? 1 : 0));
  term.fraction = static_cast<std::uint8_t>(-term.fraction);
}

std::uint32_t bits(projection_term const term) noexcept {
  /// Join the original coordinate word and fractional byte
  return static_cast<std::uint32_t>(term.whole) * 256 + term.fraction;
}

} // namespace

model_projection::model_projection(projection_parameters const supplied) : parameters{supplied} {
  /// Begin with zero cached contributions; coordinate bytecode supplies each subsequent update
}

void model_projection::set_component(std::size_t const axis, std::int16_t const value) {
  /// Replace one transformed component, retaining the native shared A/B vertical fraction
  auto &cached{contributions.at(axis)};
  auto const coefficients{parameters.axes.at(axis)};
  cached.horizontal = product(value, coefficients.horizontal);
  cached.depth = product(value, coefficients.depth);
  auto const vertical{product(value, coefficients.vertical)};
  cached.vertical = vertical.whole;
  (axis == 2 ? vertical_c_fraction : vertical_ab_fraction) = vertical.fraction;
}

void model_projection::zero_component(std::size_t const axis) {
  /// FF05/FF1F/FF3D clear a contribution and its associated shared fractional byte
  contributions.at(axis) = {};
  (axis == 2 ? vertical_c_fraction : vertical_ab_fraction) = 0;
}

void model_projection::negate_component(std::size_t const axis) {
  /// FF59/FF82/FFB0 retain the rounding already present in the cached products
  auto &cached{contributions.at(axis)};
  negate(cached.horizontal);
  negate(cached.depth);
  auto &fraction{axis == 2 ? vertical_c_fraction : vertical_ab_fraction};
  cached.vertical = static_cast<std::uint16_t>(-cached.vertical - (fraction != 0 ? 1 : 0));
  fraction = static_cast<std::uint8_t>(-fraction);
}

camera_vertex model_projection::transform() const noexcept {
  /// FC97 accumulates cached words and fractional carries before the two signed divisions
  auto horizontal{bits(parameters.horizontal)};
  auto depth{bits(parameters.depth)};
  auto vertical{bits(parameters.vertical) + vertical_ab_fraction + vertical_c_fraction};
  for(auto const &cached : contributions) {
    horizontal += bits(cached.horizontal);
    depth += bits(cached.depth);
    vertical += static_cast<std::uint32_t>(cached.vertical) * 256;
  }
  auto const signed_coordinate{[](std::uint32_t const value){ return std::bit_cast<std::int32_t>(value << 8) >> 8; }};
  return {
    .horizontal{signed_coordinate(horizontal)},
    .vertical{signed_coordinate(vertical)},
    .depth{signed_coordinate(depth)}
  };
}

projected_vertex model_projection::project() const {
  /// The direct path divides the same cached camera coordinates that the near path retains
  auto const vertex{transform()};
  return {
    .screen{project_vertex(vertex, parameters.origin)},
    .depth{static_cast<std::int16_t>(vertex.depth >> 8)}
  };
}

} // namespace darker::graphics
