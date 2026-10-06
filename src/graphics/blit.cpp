#include "graphics/blit.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace darker::graphics {

void copy_rectangle(std::span<std::uint8_t const> const source, std::span<std::uint8_t> const target,
  pixel_position const source_origin, pixel_position const destination, int const width, int const height) {
  /// Copy opaque indices, clipping both surfaces without shifting their correspondence
  if(source.size() % 320 || target.size() % 320 || source.size() > 320 * 240 || target.size() > 320 * 240) {
    throw std::invalid_argument{"blit surfaces must contain complete 320-pixel rows, at most 240"};
  }
  if(width < 0 || height < 0) throw std::invalid_argument{"negative blit dimensions"};
  auto const sx{static_cast<std::int64_t>(source_origin.x)};
  auto const sy{static_cast<std::int64_t>(source_origin.y)};
  auto const dx{static_cast<std::int64_t>(destination.x)};
  auto const dy{static_cast<std::int64_t>(destination.y)};
  auto const left{std::max({std::int64_t{0}, -sx, -dx})};
  auto const top{std::max({std::int64_t{0}, -sy, -dy})};
  auto const right{std::min({static_cast<std::int64_t>(width), 320 - sx, 320 - dx})};
  auto const bottom{std::min({static_cast<std::int64_t>(height), static_cast<std::int64_t>(source.size() / 320) - sy,
    static_cast<std::int64_t>(target.size() / 320) - dy})};
  for(auto y{top}; y < bottom; ++y) {
    for(auto x{left}; x < right; ++x) {
      target[static_cast<std::size_t>((dy + y) * 320 + dx + x)] = source[static_cast<std::size_t>((sy + y) * 320 + sx + x)];
    }
  }
}

void copy_mask(std::span<std::uint8_t const> const source, std::span<std::uint8_t> const target,
  pixel_position const source_origin, pixel_position const destination, std::span<mask_row const> const rows) {
  /// Each row's skip is relative to the common origin, not to the preceding row
  if(rows.size() > 240 || source_origin.x < -320 || source_origin.x > 320 || destination.x < -320 || destination.x > 320
    || source_origin.y < -240 || source_origin.y > 240 || destination.y < -240 || destination.y > 240) {
    throw std::invalid_argument{"mask coordinates outside supported cockpit range"};
  }
  for(std::size_t y{0}; y < rows.size(); ++y) {
    auto const &row{rows[y]};
    copy_rectangle(source, target, {.x{source_origin.x + row.skip}, .y{source_origin.y + static_cast<int>(y)}},
      {.x{destination.x + row.skip}, .y{destination.y + static_cast<int>(y)}}, row.width, 1);
  }
}

} // namespace darker::graphics
