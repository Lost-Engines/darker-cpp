#include "graphics/screen_primitives.h"
#include <algorithm>
#include <array>
#include <stdexcept>
#include "graphics/screen_layout.h"

namespace darker::graphics {
namespace {

void put_pixel(framework::render::indexed_surface target, int const x, int const y, uint8_t const colour) {
  /// The line endpoint may lie on the excluded bottom boundary
  if(x >= 0 && x < target.width && y >= 0 && y < target.height) target.pixels[y * target.stride + x] = colour;
}

} // anonymous namespace

void draw_screen_line(framework::render::indexed_surface target,
  pixel_position const &start, pixel_position const &end, uint8_t const colour) {
  /// A77B draws clipped endpoints with its original half-open vertical coverage
  auto first{start};
  auto last{end};
  auto const inside{[&](pixel_position const &point){
    return point.x >= 0 && point.x < target.width && point.y >= 0 && point.y <= target.height;
  }};
  if(!inside(first) || !inside(last)) throw std::invalid_argument{"Line endpoints exceed the display boundary"};
  if(first.x >= last.x) std::swap(first, last);
  auto const delta{last - first};
  int const width{delta.x + 1};
  int const direction{delta.y < 0 ? -1 : 1};
  int const height{delta.y * direction};
  int y{first.y - (direction < 0 ? 1 : 0)};
  int x{first.x};
  if(height < width) {
    int const rows{std::max(height, 1)};
    int const quotient{width / rows};
    int const remainder{width % rows};
    int error{rows / 2};
    for(int row{0}; row < rows; ++row) {
      int count{quotient};
      error -= remainder;
      if(error < 0) {
        ++count;
        error += rows;
      }
      for(int pixel{0}; pixel < count; ++pixel) put_pixel(target, x++, y, colour);
      y += direction;
    }
  } else {
    int const quotient{height / width};
    int const remainder{height % width};
    int error{width / 2};
    for(int column{0}; column < width; ++column) {
      int count{quotient};
      error -= remainder;
      if(error < 0) {
        ++count;
        error += width;
      }
      for(int pixel{0}; pixel < count; ++pixel) {
        put_pixel(target, x, y, colour);
        y += direction;
      }
      ++x;
    }
  }
}

bool clip_world_line(pixel_position &first, pixel_position &last, raster_viewport const viewport) {
  auto const [right, bottom]{viewport};
  /// A86D clips vertical boundaries first, then horizontal boundaries, preserving integer endpoint rounding
  if(bottom <= 0 || right < 0) throw std::invalid_argument{"Line viewport exceeds the framebuffer"};
  if(first.y > last.y) std::swap(first, last);
  if(last.y < 0) return false;
  while(true) {
    if(first.x > last.x) {
      if(first.x < 0 || last.x > right) return false;
    } else if(last.x < 0 || first.x > right) return false;
    if(first.y >= bottom) return false;
    if(first.y < 0) {
      first.x += (last.x - first.x) * -first.y / (last.y - first.y);
      first.y = 0;
      continue;
    }
    if(last.y > bottom) {
      last.x -= (last.x - first.x) * (last.y - bottom) / (last.y - first.y);
      last.y = bottom;
      continue;
    }
    break;
  }
  if(first.x >= last.x) std::swap(first, last);
  if(first.x < 0) {
    first.y += (last.y - first.y) * -first.x / (last.x - first.x);
    first.x = 0;
  }
  if(last.x > right) {
    last.y -= (last.y - first.y) * (last.x - right) / (last.x - first.x);
    last.x = right;
  }
  return first.y != bottom || last.y < bottom;
}

void draw_world_line(framework::render::indexed_surface target, pixel_position const &start, pixel_position const &end,
  uint8_t const colour, raster_viewport const viewport) {
  /// Share the original line rasteriser with the HUD after the model line's viewport clipping
  auto first{start};
  auto last{end};
  if(!viewport.fits(target.width, target.height)) throw std::invalid_argument{"Line viewport exceeds the framebuffer"};
  if(clip_world_line(first, last, viewport)) draw_screen_line(target, first, last, colour);
}

void draw_disc(framework::render::indexed_surface target, pixel_position const &centre, unsigned int radius,
  uint8_t const colour, raster_viewport const viewport) {
  auto const [right, bottom]{viewport};
  /// A5C4 constructs mirrored spans through overlapping front/back writes; preserve its small-radius asymmetry
  if(!viewport.fits(target.width, target.height)) throw std::invalid_argument{"Disc viewport exceeds the framebuffer"};
  radius = std::min(radius, 256u);
  if(radius == 0) {
    if(viewport.contains(centre.x, centre.y)) put_pixel(target, centre.x, centre.y, colour);
    return;
  }
  int const extent{static_cast<int>(radius)};
  if(centre.x + extent < 0 || centre.x - extent > right || centre.y - extent >= bottom || centre.y + extent < 0) return;
  struct span {
    int left{0};
    int right{0};
  };
  std::array<span, 1026> spans{};
  int constexpr middle{1024};
  int stack{middle - extent};
  int forward{stack};
  int backward{middle};
  int x{extent};
  int y{0};
  int error{extent / 2};
  auto const row_span{[&](int const half_width){
    return span{
      .left{std::clamp(centre.x - half_width, 0, right + 1)},
      .right{std::clamp(centre.x + half_width, 0, right + 1)}
    };
  }};
  do {
    auto const row{row_span(x)};
    do {
      spans[--stack] = row;
      spans[forward++] = row;
      ++y;
      error -= y;
    } while(error >= 0);
    error += x;
    --x;
    spans[--backward] = row_span(y);
  } while(forward < backward);
  int const top{centre.y - extent};
  if(middle - stack < bottom - top && forward != middle) {
    do {
      spans[--stack] = spans[forward++];
    } while(forward < middle);
  }
  int const count{middle - stack};
  for(int i{0}; i < count; ++i) {
    int const row{top + count - 1 - i};
    if(row < 0 || row >= bottom) continue;
    auto const bounds{spans[stack + i]};
    if(bounds.right > bounds.left) std::fill(target.pixels.begin() + row * target.stride + bounds.left, target.pixels.begin() + row * target.stride + bounds.right, colour);
  }
}

} // namespace darker::graphics
