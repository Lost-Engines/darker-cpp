#include "graphics/gouraud_polygon.h"
#include <algorithm>
#include <array>
#include <bit>
#include <stdexcept>

namespace darker::graphics {
namespace {

std::int16_t word(int const value) noexcept {
  /// Preserve the original signed word interpolation differences
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

using polygon_buffer = std::array<shaded_vertex, 260>;

std::size_t clip(std::span<shaded_vertex const> const input, polygon_buffer &output, bool const horizontal, int const boundary, bool const maximum) {
  /// AA74–AD02 clip coordinates and the palette accumulator with the same endpoint anchoring
  std::size_t count{0};
  auto const coordinate{[horizontal](shaded_vertex const point){ return horizontal ? point.x : point.y; }};
  auto const inside{[&](shaded_vertex const point){ return maximum ? coordinate(point) <= boundary : coordinate(point) >= boundary; }};
  for(std::size_t i{0}; i < input.size(); ++i) {
    auto const a{input[i]}, b{input[(i + 1) % input.size()]};
    bool const a_inside{inside(a)}, b_inside{inside(b)};
    if(a_inside) output.at(count++) = a;
    if(a_inside == b_inside) continue;
    bool const anchor_a{a_inside && (horizontal || !maximum)};
    auto const anchor{anchor_a ? a : b}, outside{anchor_a ? b : a};
    int const distance{word(boundary - coordinate(anchor))};
    int const divisor{word(coordinate(outside) - coordinate(anchor))};
    auto const interpolate{[&](int const start, int const end){
      if(divisor == 0) throw std::domain_error{"Shaded polygon clipping has zero divisor"};
      int const quotient{distance * word(end - start) / divisor};
      if(quotient < -32768 || quotient > 32767) throw std::domain_error{"Shaded polygon clipping exceeds the original quotient"};
      return word(start + quotient);
    }};
    auto const other{interpolate(horizontal ? anchor.y : anchor.x, horizontal ? outside.y : outside.x)};
    output.at(count++) = {
      .x{horizontal ? word(boundary) : other}, .y{horizontal ? other : word(boundary)},
      .shade{static_cast<std::uint16_t>(interpolate(anchor.shade, outside.shade))},
    };
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
  std::uint16_t shade{0};
  int shade_step{0};
};

void start_edge(edge_walker &edge, std::span<shaded_vertex const> const points, int const y, bool const right) {
  /// AD1D/AD8D initialise edge accumulators; shade differences include the original extra unit before division
  auto a{points[edge.index]};
  shaded_vertex b{};
  do {
    edge.index = edge.direction > 0 ? (edge.index + 1) % points.size() : (edge.index + points.size() - 1) % points.size();
    b = points[edge.index];
    if(b.y > y) break;
    a = b;
  } while(true);
  int const dx{b.x - a.x}, dy{b.y - a.y};
  int const quotient{(dx < 0 ? -dx - 1 : dx) * 128 / dy};
  edge.step = dx < 0 ? -2 * (quotient + 1) + (right ? 0 : 1) : 2 * quotient;
  edge.x = a.x + (right ? 1 : 0);
  edge.fraction = 128;
  edge.end_y = b.y;
  edge.shade = a.shade;
  edge.shade_step = word(b.shade - a.shade + 1) / dy;
}

void step_edge(edge_walker &edge) noexcept {
  /// AE69 advances both coordinate and colour accumulators before writing each row
  int const fraction{edge.fraction + (edge.step & 255)};
  edge.x += (edge.step >> 8) + (fraction >> 8);
  edge.fraction = fraction & 255;
  edge.shade = static_cast<std::uint16_t>(edge.shade + edge.shade_step);
}

void draw_span(framework::render::indexed_cockpit_framebuffer &target, int const y, int const left, int const right, int const first, int const last) {
  /// AE93 chooses per-pixel steps or repeated colour bands using integer quotient/remainder distribution
  int const width{right - left};
  if(width <= 0) return;
  int const direction{last < first ? -1 : 1};
  int const difference{(last - first) * direction};
  auto output{target.pixels.begin() + y * 320 + left};
  if(difference == 0) {
    std::fill_n(output, width, static_cast<std::uint8_t>(first));
  } else if(difference >= width) {
    int const divisor{width + 1}, step{difference / divisor}, remainder{difference % divisor};
    int error{divisor / 2}, colour{first};
    for(int i{0}; i < width; ++i) {
      *output++ = static_cast<std::uint8_t>(colour);
      error -= remainder;
      bool const borrow{error < 0};
      if(borrow) error += divisor;
      colour += direction * (step + (borrow ? 1 : 0));
    }
  } else {
    int const bands{difference + 1}, step{width / bands}, remainder{width % bands};
    int error{bands / 2};
    for(int band{0}; band < bands; ++band) {
      error -= remainder;
      bool const borrow{error < 0};
      if(borrow) error += bands;
      int const count{step + (borrow ? 1 : 0)};
      std::fill_n(output, count, static_cast<std::uint8_t>(first + direction * band));
      output += count;
    }
  }
}

} // namespace

void draw_gouraud_polygon(framework::render::indexed_cockpit_framebuffer &target,
  std::span<shaded_vertex const> const vertices, int const right, int const bottom) {
  /// Translate the original convex palette-index Gouraud path into a contiguous indexed framebuffer
  if(vertices.size() < 3) return;
  if(vertices.size() > 256 || right < 0 || right >= 320 || bottom <= 0 || bottom > 240) {
    throw std::invalid_argument{"Shaded polygon exceeds supported vertex or viewport bounds"};
  }
  auto const [leftmost, rightmost]{std::minmax_element(vertices.begin(), vertices.end(), [](auto const a, auto const b){ return a.x < b.x; })};
  auto const [top, lowest]{std::minmax_element(vertices.begin(), vertices.end(), [](auto const a, auto const b){ return a.y < b.y; })};
  if(rightmost->x < 0 || lowest->y < 0 || leftmost->x > right || top->y >= bottom) return;
  auto const index{static_cast<std::size_t>(top - vertices.begin())};
  auto const next{vertices[(index + 1) % vertices.size()]}, previous{vertices[(index + vertices.size() - 1) % vertices.size()]};
  if(back_facing({top->x, top->y}, {next.x, next.y}, {previous.x, previous.y})) return;
  polygon_buffer first{}, second{};
  std::copy(vertices.begin(), vertices.end(), first.begin());
  std::size_t count{vertices.size()};
  for(auto const plane : std::array<std::array<int, 3>, 4>{{{0, bottom, 1}, {0, 0, 0}, {1, right, 1}, {1, 0, 0}}}) {
    count = clip(std::span{first}.first(count), second, plane[0] != 0, plane[1], plane[2] != 0);
    first.swap(second);
    if(count < 3) return;
  }
  auto const points{std::span{first}.first(count)};
  auto const first_point{std::min_element(points.begin(), points.end(), [](auto const a, auto const b){ return a.y < b.y; })};
  auto const last_point{std::max_element(points.begin(), points.end(), [](auto const a, auto const b){ return a.y < b.y; })};
  auto const start{static_cast<std::size_t>(first_point - points.begin())};
  edge_walker left_edge{.index{start}, .direction{-1}, .end_y{first_point->y}};
  edge_walker right_edge{.index{start}, .end_y{first_point->y}};
  for(int y{first_point->y}; y < last_point->y; ++y) {
    if(y == left_edge.end_y) start_edge(left_edge, points, y, false);
    if(y == right_edge.end_y) start_edge(right_edge, points, y, true);
    step_edge(left_edge); step_edge(right_edge);
    int const left{std::clamp(left_edge.x, 0, right + 1)};
    int const end{std::clamp(right_edge.x, left, right + 1)};
    draw_span(target, y, left, end, left_edge.shade >> 8, right_edge.shade >> 8);
  }
}

} // namespace darker::graphics
