#include "graphics/near_clip.h"
#include <bit>
#include <limits>
#include <stdexcept>

namespace darker::graphics {
namespace {

render_geometry::accumulator wrap(render_geometry::accumulator const value) noexcept {
  /// Keep the original signed three-byte coordinate arithmetic
  return render_geometry::wrap_projection(static_cast<render_geometry::accumulator_bits>(value));
}

render_geometry::screen_coordinate word(render_geometry::accumulator const value) noexcept {
  /// Add the screen origin with the original word wrapping
  return render_geometry::wrap_screen(value);
}

render_geometry::screen_coordinate divide(render_geometry::accumulator const numerator, render_geometry::coordinate const depth, render_geometry::screen_coordinate const origin) {
  /// Preserve IDIV truncation and report the same zero-depth or quotient-overflow failure
  if(depth == 0) throw std::domain_error{"Model projection has zero depth"};
  auto const quotient{wrap(numerator) / depth};
  if(quotient < std::numeric_limits<render_geometry::screen_coordinate>::min() || quotient > std::numeric_limits<render_geometry::screen_coordinate>::max()) {
    throw std::domain_error{"Model projection exceeds the original signed quotient"};
  }
  return word(quotient + origin);
}

render_geometry::screen_coordinate intersection_coordinate(render_geometry::accumulator const value, render_geometry::screen_coordinate const origin) noexcept {
  /// 2359 saturates distant intersections before adding the screen origin; ordinary intersections shift arithmetically
  auto const high{value >> render_geometry::whole_bits};
  if(high < -render_geometry::intersection_high_word_limit || high >= render_geometry::intersection_high_word_limit) return value < 0 ? render_geometry::intersection_min : render_geometry::intersection_max;
  return word((value >> render_geometry::near_projection_shift) + origin);
}

} // anonymous namespace

screen_vertex project_vertex(camera_vertex const vertex, screen_vertex const &origin) {
  /// Project a retained camera-space vertex using its whole signed depth
  auto const depth{render_geometry::wrap_coordinate(vertex.depth >> render_geometry::fraction_bits)};
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
  auto distance{(inside.depth >> 1) - render_geometry::half_near_depth_fixed};
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

size_t clip_near_polygon(std::span<camera_vertex const> const vertices, screen_vertex const &origin, std::span<screen_vertex> const output) {
  /// 20EF emits each visible vertex then its outgoing crossing, retaining the original cyclic order
  if(vertices.empty()) return 0;
  size_t count{0};
  auto const emit{[&](screen_vertex const &point){
    if(count == output.size()) throw std::invalid_argument{"Near-clipped polygon exceeds its output buffer"};
    output[count++] = point;
  }};
  for(size_t i{0}; i < vertices.size(); ++i) {
    auto const current{vertices[i]};
    auto const next{vertices[(i + 1) % vertices.size()]};
    bool const current_inside{current.depth >= render_geometry::near_depth_fixed};
    bool const next_inside{next.depth >= render_geometry::near_depth_fixed};
    if(current_inside) emit(project_vertex(current, origin));
    if(current_inside != next_inside) emit(current_inside ? near_intersection(current, next, origin) : near_intersection(next, current, origin));
  }
  return count;
}

size_t clip_near_shaded_polygon(std::span<camera_vertex const> const vertices, std::span<uint16_t const> const shades,
  screen_vertex const &origin, std::span<shaded_vertex> const output) {
  /// 2195 retains geometric halving but interpolates colour using a separate ratio of whole depths
  if(vertices.size() != shades.size()) throw std::invalid_argument{"Near polygon colour count differs from its vertices"};
  size_t count{0};
  auto const emit{[&](screen_vertex const &point, uint16_t const shade){
    if(count == output.size()) throw std::invalid_argument{"Near shaded polygon exceeds its output buffer"};
    output[count++] = {
      .position{point.x, point.y},
      .shade{shade}
    };
  }};
  for(size_t i{0}; i < vertices.size(); ++i) {
    auto const j{(i + 1) % vertices.size()};
    bool const current_inside{vertices[i].depth >= render_geometry::near_depth_fixed}, next_inside{vertices[j].depth >= render_geometry::near_depth_fixed};
    if(current_inside) emit(project_vertex(vertices[i], origin), shades[i]);
    if(current_inside == next_inside) continue;
    auto const inside{current_inside ? i : j}, outside{current_inside ? j : i};
    auto const depth{vertices[inside].depth >> render_geometry::fraction_bits};
    auto const divisor{static_cast<uint16_t>(2 * (depth - (vertices[outside].depth >> render_geometry::fraction_bits)))};
    auto const numerator{static_cast<uint32_t>(depth - render_geometry::near_depth) * 65536};
    if(divisor == 0 || numerator / divisor > 65535) throw std::domain_error{"Near shade interpolation exceeds the original quotient"};
    auto const fraction{static_cast<int16_t>(numerator / divisor)};
    int const delta{std::bit_cast<int16_t>(static_cast<uint16_t>((shades[outside] - shades[inside]) * 2))};
    auto const shade{static_cast<uint16_t>(shades[inside] + ((delta * fraction) >> 16))};
    emit(near_intersection(vertices[inside], vertices[outside], origin), shade);
  }
  return count;
}

} // namespace darker::graphics
