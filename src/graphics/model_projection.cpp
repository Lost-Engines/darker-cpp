#include "graphics/model_projection.h"
#include <bit>
#include <limits>
#include <stdexcept>

namespace darker::graphics {
namespace {

projection_term product(std::int16_t const value, std::int16_t const coefficient) noexcept {
  /// FD04/FD30/FD5C cache the upper word and middle byte of each signed product
  auto const bits{static_cast<std::uint32_t>(static_cast<std::int32_t>(value) * coefficient)};
  return {.whole{static_cast<std::uint16_t>(bits >> 16)}, .fraction{static_cast<std::uint8_t>(bits >> 8)}};
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

std::int16_t divide(std::uint32_t const numerator, std::int16_t const depth, std::int16_t const origin) {
  /// FC97 divides a signed 24-bit numerator, then wraps the screen-origin addition
  auto const signed_numerator{std::bit_cast<std::int32_t>((numerator & 0xffffffu) << 8) >> 8};
  if(depth == 0) throw std::domain_error{"Model projection has zero depth"};
  auto const quotient{signed_numerator / depth};
  if(quotient < std::numeric_limits<std::int16_t>::min() || quotient > std::numeric_limits<std::int16_t>::max()) {
    throw std::domain_error{"Model projection exceeds the original signed quotient"};
  }
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(quotient + origin));
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

projected_vertex model_projection::project() const {
  /// FC97 accumulates cached words and fractional carries before the two signed divisions
  auto horizontal{bits(parameters.horizontal)};
  auto depth{bits(parameters.depth)};
  auto vertical{bits(parameters.vertical) + vertical_ab_fraction + vertical_c_fraction};
  for(auto const &cached : contributions) {
    horizontal += bits(cached.horizontal);
    depth += bits(cached.depth);
    vertical += static_cast<std::uint32_t>(cached.vertical) * 256;
  }
  auto const divisor{std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(depth >> 8))};
  return {
    .screen{.x{divide(horizontal, divisor, parameters.origin.x)}, .y{divide(vertical, divisor, parameters.origin.y)}},
    .depth{divisor},
  };
}

} // namespace darker::graphics
