#include "graphics/gouraud_polygon.h"
#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <stdexcept>
#include "graphics/raster_arithmetic.h"

namespace darker::graphics {
namespace {

render_geometry::screen_coordinate word(int const value) noexcept {
  /// Preserve the original signed word interpolation differences
  return render_geometry::wrap_screen(value);
}

using polygon_buffer = std::array<shaded_vertex, clipped_polygon_vertex_limit>;

size_t clip(std::span<shaded_vertex const> const input, polygon_buffer &output, bool const horizontal, int const boundary, bool const maximum) {
  /// AA74–AD02 clip coordinates and the palette accumulator with the same endpoint anchoring
  size_t count{0};
  auto const coordinate{[horizontal](shaded_vertex const &point){
    return horizontal ? point.position.x : point.position.y;
  }};
  auto const inside{[&](shaded_vertex const &point){
    return maximum ? coordinate(point) <= boundary : coordinate(point) >= boundary;
  }};
  for(size_t i{0}; i < input.size(); ++i) {
    auto const &a{input[i]}, &b{input[(i + 1) % input.size()]};
    bool const a_inside{inside(a)}, b_inside{inside(b)};
    if(a_inside) output.at(count++) = a;
    if(a_inside == b_inside) continue;
    bool const anchor_a{a_inside && (horizontal || !maximum)};
    auto const &anchor{anchor_a ? a : b}, &outside{anchor_a ? b : a};
    int const distance{word(boundary - coordinate(anchor))};
    int const divisor{word(coordinate(outside) - coordinate(anchor))};
    auto const interpolate{[&](int const start, int const end, auto const wrap){
      if(divisor == 0) throw std::domain_error{"Shaded polygon clipping has zero divisor"};
      int const quotient{distance * wrap(end - start) / divisor};
      if(quotient < std::numeric_limits<raster_arithmetic::division_quotient>::min() || quotient > std::numeric_limits<raster_arithmetic::division_quotient>::max()) throw std::domain_error{"Shaded polygon clipping exceeds the original quotient"};
      return wrap(start + quotient);
    }};
    auto const other{interpolate(horizontal ? anchor.position.y : anchor.position.x, horizontal ? outside.position.y : outside.position.x, word)};
    output.at(count++) = {
      .position{horizontal ? word(boundary) : other, horizontal ? other : word(boundary)},
      .shade{static_cast<uint16_t>(interpolate(anchor.shade, outside.shade, raster_arithmetic::wrap_palette))},
    };
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
  raster_arithmetic::palette_accumulator shade{0};
  int shade_step{0};
};

void start_edge(edge_walker &edge, std::span<shaded_vertex const> const points, int const y, bool const right) {
  /// AD1D/AD8D initialise edge accumulators; shade differences include the original extra unit before division
  auto a{points[edge.index]};
  shaded_vertex b{};
  do {
    edge.index = edge.direction > 0 ? (edge.index + 1) % points.size() : (edge.index + points.size() - 1) % points.size();
    b = points[edge.index];
    if(b.position.y > y) break;
    a = b;
  } while(true);
  auto const delta{vec2<int>{b.position} - vec2<int>{a.position}};
  int const quotient{(delta.x < 0 ? -delta.x - 1 : delta.x) * raster_arithmetic::edge_start_fraction / delta.y};
  edge.step = delta.x < 0 ? -2 * (quotient + 1) + (right ? 0 : 1) : 2 * quotient;
  edge.x = a.position.x + (right ? 1 : 0);
  edge.fraction = raster_arithmetic::edge_start_fraction;
  edge.end_y = b.position.y;
  edge.shade = a.shade;
  edge.shade_step = raster_arithmetic::wrap_palette(b.shade - a.shade + 1) / delta.y;
}

void step_edge(edge_walker &edge) noexcept {
  /// AE69 advances both coordinate and colour accumulators before writing each row
  int const fraction{edge.fraction + (edge.step & raster_arithmetic::edge_fraction_mask)};
  edge.x += (edge.step >> raster_arithmetic::edge_fraction_bits) + (fraction >> raster_arithmetic::edge_fraction_bits);
  edge.fraction = fraction & raster_arithmetic::edge_fraction_mask;
  edge.shade = static_cast<uint16_t>(edge.shade + edge.shade_step);
}

void draw_span(framework::render::indexed_surface target, int const y, int const left, int const right, int const first, int const last) {
  /// AE93 chooses per-pixel steps or repeated colour bands using integer quotient/remainder distribution
  int const width{right - left};
  if(width <= 0) return;
  int const direction{last < first ? -1 : 1};
  int const difference{(last - first) * direction};
  auto output{target.pixels.begin() + y * target.stride + left};
  if(difference == 0) {
    std::fill_n(output, width, static_cast<uint8_t>(first));
  } else if(difference >= width) {
    int const divisor{width + 1}, step{difference / divisor}, remainder{difference % divisor};
    int error{divisor / 2}, colour{first};
    for(int i{0}; i < width; ++i) {
      *output++ = static_cast<uint8_t>(colour);
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
      std::fill_n(output, count, static_cast<uint8_t>(first + direction * band));
      output += count;
    }
  }
}

void rasterise(framework::render::indexed_surface target, std::span<shaded_vertex const> const points, int const right) {
  /// Walk the original edges and shade spans after visibility and clipping are resolved
  auto const first_point{std::min_element(points.begin(), points.end(), [](auto const &a, auto const &b){
    return a.position.y < b.position.y;
  })};
  auto const last_point{std::max_element(points.begin(), points.end(), [](auto const &a, auto const &b){
    return a.position.y < b.position.y;
  })};
  auto const start{static_cast<size_t>(first_point - points.begin())};
  edge_walker left_edge{
    .index{start},
    .direction{-1},
    .end_y{first_point->position.y}
  };
  edge_walker right_edge{
    .index{start},
    .end_y{first_point->position.y}
  };
  for(int y{first_point->position.y}; y < last_point->position.y; ++y) {
    if(y == left_edge.end_y) start_edge(left_edge, points, y, false);
    if(y == right_edge.end_y) start_edge(right_edge, points, y, true);
    step_edge(left_edge);
    step_edge(right_edge);
    int const left{std::clamp(left_edge.x, 0, right + 1)};
    int const end{std::clamp(right_edge.x, left, right + 1)};
    draw_span(target, y, left, end, left_edge.shade >> 8, right_edge.shade >> 8);
  }
}

} // anonymous namespace

void draw_gouraud_polygon(framework::render::indexed_surface target,
  std::span<shaded_vertex const> const vertices, raster_viewport const viewport) {
  auto const [right, bottom]{viewport};
  /// Translate the original convex palette-index Gouraud path into a contiguous indexed framebuffer
  if(vertices.size() < 3) return;
  if(vertices.size() > polygon_vertex_limit || !viewport.fits(target.width, target.height)) {
    throw std::invalid_argument{"Shaded polygon exceeds supported vertex or viewport bounds"};
  }
  auto const [leftmost, rightmost]{std::minmax_element(vertices.begin(), vertices.end(), [](auto const &a, auto const &b){
    return a.position.x < b.position.x;
  })};
  auto const [top, lowest]{std::minmax_element(vertices.begin(), vertices.end(), [](auto const &a, auto const &b){
    return a.position.y < b.position.y;
  })};
  if(rightmost->position.x < 0 || lowest->position.y < 0 || leftmost->position.x > right || top->position.y >= bottom) return;
  if(top->position.y == lowest->position.y) return; // no scanlines, including subpixel distant faces
  auto const index{static_cast<size_t>(top - vertices.begin())};
  auto const &next{vertices[(index + 1) % vertices.size()]}, &previous{vertices[(index + vertices.size() - 1) % vertices.size()]};
  if(back_facing(top->position, next.position, previous.position)) return;
  // Interior polygons need neither clipping nor temporary copies. Distant faces
  // often occupy only a few pixels, so buffer housekeeping otherwise dominates.
  if(leftmost->position.x >= 0 && rightmost->position.x <= right
    && top->position.y >= 0 && lowest->position.y <= bottom) {
    rasterise(target, vertices, right);
    return;
  }
  polygon_buffer first{}, second{};
  auto points{vertices};
  auto *output{&first};
  for(auto const plane : viewport_clip_planes(viewport)) {
    auto const count{clip(points, *output, plane.horizontal, plane.boundary, plane.maximum)};
    if(count < 3) return;
    points = std::span{*output}.first(count);
    output = output == &first ? &second : &first;
  }
  rasterise(target, points, right);
}

} // namespace darker::graphics
