#include "graphics/flat_polygon.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include "graphics/raster_arithmetic.h"
#include "graphics/screen_layout.h"

namespace darker::graphics {
namespace {

using polygon_buffer = std::array<screen_vertex, clipped_polygon_vertex_limit>;

render_geometry::screen_coordinate word(int const value) noexcept {
  /// Preserve signed word coordinates at clipping boundaries
  return render_geometry::wrap_screen(value);
}

size_t clip(std::span<screen_vertex const> const input, polygon_buffer &output, bool const horizontal, int const boundary, bool const maximum) {
  /// Bottom clipping anchors at the next vertex; the other planes anchor at the inside endpoint
  size_t count{0};
  auto const coordinate{[horizontal](screen_vertex const &point){
    return horizontal ? point.x : point.y;
  }};
  auto const inside{[&](screen_vertex const &point){
    return maximum ? coordinate(point) <= boundary : coordinate(point) >= boundary;
  }};
  for(size_t i{0}; i < input.size(); ++i) {
    auto const a{input[i]};
    auto const b{input[(i + 1) % input.size()]};
    bool const a_inside{inside(a)};
    bool const b_inside{inside(b)};
    if(a_inside) output.at(count++) = a;
    if(a_inside == b_inside) continue;
    bool const anchor_a{a_inside && (horizontal || !maximum)};
    auto const anchor{anchor_a ? a : b};
    auto const outside{anchor_a ? b : a};
    auto const distance{word(boundary - coordinate(anchor))};
    auto const divisor{word(coordinate(outside) - coordinate(anchor))};
    auto const delta{word(horizontal ? outside.y - anchor.y : outside.x - anchor.x)};
    if(divisor == 0) throw std::domain_error{"Polygon clipping produces an original division fault"};
    auto const quotient{static_cast<int32_t>(distance) * delta / divisor};
    if(quotient < std::numeric_limits<raster_arithmetic::division_quotient>::min() || quotient > std::numeric_limits<raster_arithmetic::division_quotient>::max()) throw std::domain_error{"Polygon clipping exceeds the original signed quotient"};
    auto const interpolated{word((horizontal ? anchor.y : anchor.x) + quotient)};
    output.at(count++) = horizontal ? screen_vertex{word(boundary), interpolated} : screen_vertex{interpolated, word(boundary)};
  }
  return count;
}

struct edge_walker {
  size_t index{0};
  int direction{1};
  int end_y{0};
  int x{0};
  int fraction{raster_arithmetic::edge_start_fraction};
  int step{0};
};

void start_edge(edge_walker &edge, std::span<screen_vertex const> const points, int const y, bool const right) {
  /// A47F/A4DB skip horizontal edges and reset the original 8-bit fractional accumulator
  auto a{points[edge.index]};
  screen_vertex b{};
  do {
    edge.index = edge.direction > 0 ? (edge.index + 1) % points.size() : (edge.index + points.size() - 1) % points.size();
    b = points[edge.index];
    if(b.y > y) break;
    a = b;
  } while(true);
  auto const delta{vec2<int>{b} - vec2<int>{a}};
  int const quotient{(delta.x < 0 ? -delta.x - 1 : delta.x) * raster_arithmetic::edge_start_fraction / delta.y};
  edge.step = delta.x < 0 ? -2 * (quotient + 1) + (right ? 0 : 1) : 2 * quotient;
  edge.x = a.x + (right ? 1 : 0);
  edge.fraction = raster_arithmetic::edge_start_fraction;
  edge.end_y = b.y;
}

void step_edge(edge_walker &edge) noexcept {
  /// Advance before drawing, preserving the native half-unit starting bias
  int const fraction{edge.fraction + (edge.step & raster_arithmetic::edge_fraction_mask)};
  edge.x += (edge.step >> raster_arithmetic::edge_fraction_bits) + (fraction >> raster_arithmetic::edge_fraction_bits);
  edge.fraction = fraction & raster_arithmetic::edge_fraction_mask;
}

} // anonymous namespace

bool back_facing(screen_vertex const &origin, screen_vertex const &next, screen_vertex const &previous) noexcept {
  /// Preserve the signed high word of the native cross product, including word differences and subtraction wrap
  auto const cross{static_cast<uint32_t>(word(next.x - origin.x) * word(previous.y - origin.y))
    - static_cast<uint32_t>(word(next.y - origin.y) * word(previous.x - origin.x))};
  return (cross & 0x80000000u) != 0;
}

void draw_flat_polygon(framework::render::indexed_surface target, std::span<screen_vertex const> const vertices,
  uint8_t const colour, raster_viewport const viewport) {
  auto const [right, bottom]{viewport};
  /// Translate A1B6's convex flat-fill path to indexed pixels; VGA plane masks become contiguous spans
  if(vertices.size() < 3) return;
  if(vertices.size() > polygon_vertex_limit || !viewport.fits(target.width, target.height)) {
    throw std::invalid_argument{"Flat polygon exceeds the supported vertex or viewport bounds"};
  }
  auto const [leftmost, rightmost]{std::minmax_element(vertices.begin(), vertices.end(), [](auto const &a, auto const &b){
    return a.x < b.x;
  })};
  auto const [top, lowest]{std::minmax_element(vertices.begin(), vertices.end(), [](auto const &a, auto const &b){
    return a.y < b.y;
  })};
  if(rightmost->x < 0 || lowest->y < 0 || leftmost->x > right || top->y >= bottom) return;
  auto const index{static_cast<size_t>(top - vertices.begin())};
  auto const next{vertices[(index + 1) % vertices.size()]};
  auto const previous{vertices[(index + vertices.size() - 1) % vertices.size()]};
  if(back_facing(*top, next, previous)) return;

  polygon_buffer first{};
  polygon_buffer second{};
  std::copy(vertices.begin(), vertices.end(), first.begin());
  size_t count{vertices.size()};
  for(auto const plane : viewport_clip_planes(viewport)) {
    count = clip(std::span{first}.first(count), second, plane.horizontal, plane.boundary, plane.maximum);
    first.swap(second);
    if(count < 3) return;
  }
  auto const points{std::span{first}.first(count)};
  auto const first_point{std::min_element(points.begin(), points.end(), [](auto const &a, auto const &b){
    return a.y < b.y;
  })};
  auto const last_point{std::max_element(points.begin(), points.end(), [](auto const &a, auto const &b){
    return a.y < b.y;
  })};
  auto const start{static_cast<size_t>(first_point - points.begin())};
  edge_walker left_edge{
    .index{start},
    .direction{-1},
    .end_y{first_point->y}
  };
  edge_walker right_edge{
    .index{start},
    .end_y{first_point->y}
  };
  for(int y{first_point->y}; y < last_point->y; ++y) {
    if(y == left_edge.end_y) start_edge(left_edge, points, y, false);
    if(y == right_edge.end_y) start_edge(right_edge, points, y, true);
    step_edge(left_edge);
    step_edge(right_edge);
    auto const left{std::clamp(left_edge.x, 0, right + 1)};
    auto const end{std::clamp(right_edge.x, left, right + 1)};
    std::fill(target.pixels.begin() + y * target.stride + left, target.pixels.begin() + y * target.stride + end, colour);
  }
}

} // namespace darker::graphics
