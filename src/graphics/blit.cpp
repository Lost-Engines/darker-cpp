#include "graphics/blit.h"
#include "graphics/screen_layout.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace darker::graphics {

void copy_rectangle(std::span<uint8_t const> const source, std::span<uint8_t> const target,
  pixel_position const &source_origin, pixel_position const &destination, int const width, int const height) {
  /// Copy opaque indices, clipping both surfaces without shifting their correspondence
  if(source.size() % display_layout::width || target.size() % display_layout::width || source.size() > display_layout::width * display_layout::height || target.size() > display_layout::width * display_layout::height) {
    throw std::invalid_argument{"blit surfaces must contain complete 320-pixel rows, at most 240"};
  }
  if(width < 0 || height < 0) throw std::invalid_argument{"negative blit dimensions"};
  vec2<int64_t> const source_offset{source_origin};
  vec2<int64_t> const target_offset{destination};
  auto const left{std::max({int64_t{0}, -source_offset.x, -target_offset.x})};
  auto const top{std::max({int64_t{0}, -source_offset.y, -target_offset.y})};
  auto const right{std::min({static_cast<int64_t>(width), display_layout::width - source_offset.x, display_layout::width - target_offset.x})};
  auto const bottom{std::min({static_cast<int64_t>(height), static_cast<int64_t>(source.size() / display_layout::width) - source_offset.y,
    static_cast<int64_t>(target.size() / display_layout::width) - target_offset.y})};
  for(auto y{top}; y < bottom; ++y) {
    for(auto x{left}; x < right; ++x) {
      target[static_cast<size_t>((target_offset.y + y) * display_layout::width + target_offset.x + x)] = source[static_cast<size_t>((source_offset.y + y) * display_layout::width + source_offset.x + x)];
    }
  }
}

void copy_mask(std::span<uint8_t const> const source, std::span<uint8_t> const target,
  pixel_position const &source_origin, pixel_position const &destination, std::span<mask_row const> const rows) {
  /// Each row's skip is relative to the common origin, not to the preceding row
  if(rows.size() > display_layout::height || source_origin.x < -display_layout::width || source_origin.x > display_layout::width || destination.x < -display_layout::width || destination.x > display_layout::width
    || source_origin.y < -display_layout::height || source_origin.y > display_layout::height || destination.y < -display_layout::height || destination.y > display_layout::height) {
    throw std::invalid_argument{"mask coordinates outside supported cockpit range"};
  }
  for(size_t y{0}; y < rows.size(); ++y) {
    auto const &row{rows[y]};
    auto const offset{pixel_position{row.skip, static_cast<int>(y)}};
    copy_rectangle(source, target, source_origin + offset, destination + offset, row.width, 1);
  }
}

} // namespace darker::graphics
