#include "graphics/blit.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace darker::graphics {

void copy_rectangle(framework::render::const_indexed_surface const source, framework::render::indexed_surface const target,
  pixel_position const &source_origin, pixel_position const &destination, int const width, int const height) {
  /// Copy opaque indices, clipping both surfaces without shifting their correspondence
  if(width < 0 || height < 0) throw std::invalid_argument{"negative blit dimensions"};
  vec2<int64_t> const source_offset{source_origin};
  vec2<int64_t> const target_offset{destination};
  auto const left{std::max({int64_t{0}, -source_offset.x, -target_offset.x})};
  auto const top{std::max({int64_t{0}, -source_offset.y, -target_offset.y})};
  auto const right{std::min({static_cast<int64_t>(width), source.width - source_offset.x, target.width - target_offset.x})};
  auto const bottom{std::min({static_cast<int64_t>(height), static_cast<int64_t>(source.height) - source_offset.y,
    static_cast<int64_t>(target.height) - target_offset.y})};
  for(auto y{top}; y < bottom; ++y) {
    auto const source_row{source.row(static_cast<int>(source_offset.y + y))};
    auto const target_row{target.row(static_cast<int>(target_offset.y + y))};
    for(auto x{left}; x < right; ++x) {
      target_row[static_cast<size_t>(target_offset.x + x)] = source_row[static_cast<size_t>(source_offset.x + x)];
    }
  }
}

void copy_mask(framework::render::const_indexed_surface const source, framework::render::indexed_surface const target,
  pixel_position const &source_origin, pixel_position const &destination, std::span<mask_row const> const rows) {
  /// Each row's skip is relative to the common origin, not to the preceding row
  if(rows.size() > static_cast<size_t>(std::numeric_limits<int>::max())
    || source_origin.y > std::numeric_limits<int>::max() - static_cast<int>(rows.size())
    || destination.y > std::numeric_limits<int>::max() - static_cast<int>(rows.size())
    || source_origin.x > std::numeric_limits<int>::max() - std::numeric_limits<uint8_t>::max()
    || destination.x > std::numeric_limits<int>::max() - std::numeric_limits<uint8_t>::max()) {
    throw std::invalid_argument{"Mask coordinates exceed pixel position range"};
  }
  for(size_t y{0}; y < rows.size(); ++y) {
    auto const &row{rows[y]};
    auto const offset{pixel_position{row.skip, static_cast<int>(y)}};
    copy_rectangle(source, target, source_origin + offset, destination + offset, row.width, 1);
  }
}

} // namespace darker::graphics
