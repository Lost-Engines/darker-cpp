#include "graphics/flat_polygon.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <stdexcept>

namespace darker::graphics {
namespace {

using polygon_buffer = std::array<screen_vertex, 260>;

std::int16_t word(int const value) noexcept {
  /// Preserve signed word coordinates at clipping boundaries
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

std::size_t clip(std::span<screen_vertex const> const input, polygon_buffer &output, bool const horizontal, int const boundary, bool const maximum) {
  /// Bottom clipping anchors at the next vertex; the other planes anchor at the inside endpoint
  std::size_t count{0};
  auto const coordinate{[horizontal](screen_vertex const &point){ return horizontal ? point.x : point.y; }};
  auto const inside{[&](screen_vertex const &point){ return maximum ? coordinate(point) <= boundary : coordinate(point) >= boundary; }};
  for(std::size_t i{0}; i < input.size(); ++i) {
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
    auto const quotient{static_cast<std::int32_t>(distance) * delta / divisor};
    if(quotient < -32768 || quotient > 32767) throw std::domain_error{"Polygon clipping exceeds the original signed quotient"};
    auto const interpolated{word((horizontal ? anchor.y : anchor.x) + quotient)};
    output.at(count++) = horizontal ? screen_vertex{word(boundary), interpolated} : screen_vertex{interpolated, word(boundary)};
  }
  return count;
}

struct edge_walker {
  std::size_t index{0};
  int direction{1};
  int end_y{0};
  int x{0};
  int fraction{128};
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
  int const quotient{(delta.x < 0 ? -delta.x - 1 : delta.x) * 128 / delta.y};
  edge.step = delta.x < 0 ? -2 * (quotient + 1) + (right ? 0 : 1) : 2 * quotient;
  edge.x = a.x + (right ? 1 : 0);
  edge.fraction = 128;
  edge.end_y = b.y;
}

void step_edge(edge_walker &edge) noexcept {
  /// Advance before drawing, preserving the native half-unit starting bias
  int const fraction{edge.fraction + (edge.step & 255)};
  edge.x += (edge.step >> 8) + (fraction >> 8);
  edge.fraction = fraction & 255;
}

} // namespace

bool back_facing(screen_vertex const &origin, screen_vertex const &next, screen_vertex const &previous) noexcept {
  /// Preserve the signed high word of the native cross product, including word differences and subtraction wrap
  auto const cross{static_cast<std::uint32_t>(word(next.x - origin.x) * word(previous.y - origin.y))
    - static_cast<std::uint32_t>(word(next.y - origin.y) * word(previous.x - origin.x))};
  return (cross & 0x80000000u) != 0;
}

void draw_flat_polygon(framework::render::indexed_cockpit_framebuffer &target, std::span<screen_vertex const> const vertices,
  std::uint8_t const colour, int const right, int const bottom) {
  /// Translate A1B6's convex flat-fill path to indexed pixels; VGA plane masks become contiguous spans
  if(vertices.size() < 3) return;
  if(vertices.size() > 256 || right < 0 || right >= 320 || bottom <= 0 || bottom > 240) {
    throw std::invalid_argument{"Flat polygon exceeds the supported vertex or viewport bounds"};
  }
  auto const [leftmost, rightmost]{std::minmax_element(vertices.begin(), vertices.end(), [](auto const &a, auto const &b){ return a.x < b.x; })};
  auto const [top, lowest]{std::minmax_element(vertices.begin(), vertices.end(), [](auto const &a, auto const &b){ return a.y < b.y; })};
  if(rightmost->x < 0 || lowest->y < 0 || leftmost->x > right || top->y >= bottom) return;
  auto const index{static_cast<std::size_t>(top - vertices.begin())};
  auto const next{vertices[(index + 1) % vertices.size()]};
  auto const previous{vertices[(index + vertices.size() - 1) % vertices.size()]};
  if(back_facing(*top, next, previous)) return;

  polygon_buffer first{};
  polygon_buffer second{};
  std::copy(vertices.begin(), vertices.end(), first.begin());
  std::size_t count{vertices.size()};
  for(auto const plane : std::array<std::array<int, 3>, 4>{{{0, bottom, 1}, {0, 0, 0}, {1, right, 1}, {1, 0, 0}}}) {
    count = clip(std::span{first}.first(count), second, plane[0] != 0, plane[1], plane[2] != 0);
    first.swap(second);
    if(count < 3) return;
  }
  auto const points{std::span{first}.first(count)};
  auto const first_point{std::min_element(points.begin(), points.end(), [](auto const &a, auto const &b){ return a.y < b.y; })};
  auto const last_point{std::max_element(points.begin(), points.end(), [](auto const &a, auto const &b){ return a.y < b.y; })};
  auto const start{static_cast<std::size_t>(first_point - points.begin())};
  edge_walker left_edge{.index{start}, .direction{-1}, .end_y{first_point->y}};
  edge_walker right_edge{.index{start}, .end_y{first_point->y}};
  for(int y{first_point->y}; y < last_point->y; ++y) {
    if(y == left_edge.end_y) start_edge(left_edge, points, y, false);
    if(y == right_edge.end_y) start_edge(right_edge, points, y, true);
    step_edge(left_edge);
    step_edge(right_edge);
    auto const left{std::clamp(left_edge.x, 0, right + 1)};
    auto const end{std::clamp(right_edge.x, left, right + 1)};
    std::fill(target.pixels.begin() + y * 320 + left, target.pixels.begin() + y * 320 + end, colour);
  }
}

} // namespace darker::graphics
