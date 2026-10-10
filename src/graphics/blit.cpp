#include "graphics/blit.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace darker::graphics {

void copy_rectangle(std::span<uint8_t const> const source, std::span<uint8_t> const target,
  pixel_position const &source_origin, pixel_position const &destination, int const width, int const height) {
  /// Copy opaque indices, clipping both surfaces without shifting their correspondence
  if(source.size() % 320 || target.size() % 320 || source.size() > 320 * 240 || target.size() > 320 * 240) {
    throw std::invalid_argument{"blit surfaces must contain complete 320-pixel rows, at most 240"};
  }
  if(width < 0 || height < 0) throw std::invalid_argument{"negative blit dimensions"};
  vec2<int64_t> const source_offset{source_origin};
  vec2<int64_t> const target_offset{destination};
  auto const left{std::max({int64_t{0}, -source_offset.x, -target_offset.x})};
  auto const top{std::max({int64_t{0}, -source_offset.y, -target_offset.y})};
  auto const right{std::min({static_cast<int64_t>(width), 320 - source_offset.x, 320 - target_offset.x})};
  auto const bottom{std::min({static_cast<int64_t>(height), static_cast<int64_t>(source.size() / 320) - source_offset.y,
    static_cast<int64_t>(target.size() / 320) - target_offset.y})};
  for(auto y{top}; y < bottom; ++y) {
    for(auto x{left}; x < right; ++x) {
      target[static_cast<size_t>((target_offset.y + y) * 320 + target_offset.x + x)] = source[static_cast<size_t>((source_offset.y + y) * 320 + source_offset.x + x)];
    }
  }
}

void copy_mask(std::span<uint8_t const> const source, std::span<uint8_t> const target,
  pixel_position const &source_origin, pixel_position const &destination, std::span<mask_row const> const rows) {
  /// Each row's skip is relative to the common origin, not to the preceding row
  if(rows.size() > 240 || source_origin.x < -320 || source_origin.x > 320 || destination.x < -320 || destination.x > 320
    || source_origin.y < -240 || source_origin.y > 240 || destination.y < -240 || destination.y > 240) {
    throw std::invalid_argument{"mask coordinates outside supported cockpit range"};
  }
  for(size_t y{0}; y < rows.size(); ++y) {
    auto const &row{rows[y]};
    auto const offset{pixel_position{row.skip, static_cast<int>(y)}};
    copy_rectangle(source, target, source_origin + offset, destination + offset, row.width, 1);
  }
}

} // namespace darker::graphics
