#include "graphics/near_clip.h"
#include <bit>
#include <limits>
#include <stdexcept>

namespace darker::graphics {
namespace {

std::int32_t wrap(std::int32_t const value) noexcept {
  /// Keep the original signed three-byte coordinate arithmetic
  return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value) << 8) >> 8;
}

std::int16_t word(int const value) noexcept {
  /// Add the screen origin with the original word wrapping
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

std::int16_t divide(std::int32_t const numerator, std::int16_t const depth, std::int16_t const origin) {
  /// Preserve IDIV truncation and report the same zero-depth or quotient-overflow failure
  if(depth == 0) throw std::domain_error{"Model projection has zero depth"};
  auto const quotient{wrap(numerator) / depth};
  if(quotient < std::numeric_limits<std::int16_t>::min() || quotient > std::numeric_limits<std::int16_t>::max()) {
    throw std::domain_error{"Model projection exceeds the original signed quotient"};
  }
  return word(quotient + origin);
}

std::int16_t intersection_coordinate(std::int32_t const value, std::int16_t const origin) noexcept {
  /// 2359 saturates distant intersections before adding the screen origin; ordinary intersections shift arithmetically
  auto const high{value >> 16};
  if(high < -15 || high >= 15) return value < 0 ? -16383 : 16382;
  return word((value >> 5) + origin);
}

} // namespace

screen_vertex project_vertex(camera_vertex const vertex, screen_vertex const &origin) {
  /// Project a retained camera-space vertex using its whole signed depth
  auto const depth{word(vertex.depth >> 8)};
  return {divide(vertex.horizontal, depth, origin.x), divide(vertex.vertical, depth, origin.y)};
}

screen_vertex near_intersection(camera_vertex const inside, camera_vertex const outside, screen_vertex const &origin) {
  /// 22B4 searches the depth-32 crossing with ordered arithmetic halvings, retaining the native asymmetric rounding
  auto horizontal{inside.horizontal};
  auto vertical{inside.vertical};
  auto horizontal_step{wrap(inside.horizontal - outside.horizontal)};
  auto vertical_step{wrap(inside.vertical - outside.vertical)};
  auto depth_step{wrap(inside.depth - outside.depth + 1) >> 1};
  if(depth_step <= 0) throw std::domain_error{"Near-plane edge exceeds the original signed depth span"};
  auto distance{(inside.depth >> 1) - 16 * 256};
  while(distance != 0) {
    depth_step >>= 1;
    if(depth_step == 0) break;
    horizontal_step >>= 1;
    vertical_step >>= 1;
    if(distance > 0) {
      horizontal = wrap(horizontal - horizontal_step);
      vertical = wrap(vertical - vertical_step);
      distance = wrap(distance - depth_step);
    } else {
      horizontal = wrap(horizontal + horizontal_step);
      vertical = wrap(vertical + vertical_step);
      distance = wrap(distance + depth_step);
    }
  }
  return {intersection_coordinate(horizontal, origin.x), intersection_coordinate(vertical, origin.y)};
}

std::size_t clip_near_polygon(std::span<camera_vertex const> const vertices, screen_vertex const &origin, std::span<screen_vertex> const output) {
  /// 20EF emits each visible vertex then its outgoing crossing, retaining the original cyclic order
  if(vertices.empty()) return 0;
  std::size_t count{0};
  auto const emit{[&](screen_vertex const &point){
    if(count == output.size()) throw std::invalid_argument{"Near-clipped polygon exceeds its output buffer"};
    output[count++] = point;
  }};
  for(std::size_t i{0}; i < vertices.size(); ++i) {
    auto const current{vertices[i]};
    auto const next{vertices[(i + 1) % vertices.size()]};
    bool const current_inside{current.depth >= 32 * 256};
    bool const next_inside{next.depth >= 32 * 256};
    if(current_inside) emit(project_vertex(current, origin));
    if(current_inside != next_inside) emit(current_inside ? near_intersection(current, next, origin) : near_intersection(next, current, origin));
  }
  return count;
}

std::size_t clip_near_shaded_polygon(std::span<camera_vertex const> const vertices, std::span<std::uint16_t const> const shades,
  screen_vertex const &origin, std::span<shaded_vertex> const output) {
  /// 2195 retains geometric halving but interpolates colour using a separate ratio of whole depths
  if(vertices.size() != shades.size()) throw std::invalid_argument{"Near polygon colour count differs from its vertices"};
  std::size_t count{0};
  auto const emit{[&](screen_vertex const &point, std::uint16_t const shade){
    if(count == output.size()) throw std::invalid_argument{"Near shaded polygon exceeds its output buffer"};
    output[count++] = {.x{point.x}, .y{point.y}, .shade{shade}};
  }};
  for(std::size_t i{0}; i < vertices.size(); ++i) {
    auto const j{(i + 1) % vertices.size()};
    bool const current_inside{vertices[i].depth >= 32 * 256}, next_inside{vertices[j].depth >= 32 * 256};
    if(current_inside) emit(project_vertex(vertices[i], origin), shades[i]);
    if(current_inside == next_inside) continue;
    auto const inside{current_inside ? i : j}, outside{current_inside ? j : i};
    auto const depth{vertices[inside].depth >> 8};
    auto const divisor{static_cast<std::uint16_t>(2 * (depth - (vertices[outside].depth >> 8)))};
    auto const numerator{static_cast<std::uint32_t>(depth - 32) * 65536};
    if(divisor == 0 || numerator / divisor > 65535) throw std::domain_error{"Near shade interpolation exceeds the original quotient"};
    auto const fraction{static_cast<std::int16_t>(numerator / divisor)};
    int const delta{word((shades[outside] - shades[inside]) * 2)};
    auto const shade{static_cast<std::uint16_t>(shades[inside] + ((delta * fraction) >> 16))};
    emit(near_intersection(vertices[inside], vertices[outside], origin), shade);
  }
  return count;
}

} // namespace darker::graphics
