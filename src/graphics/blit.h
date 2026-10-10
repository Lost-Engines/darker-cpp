#pragma once

#include <cstdint>
#include <span>
#include "vectorstorm/vector/vector2.h"

namespace darker::graphics {

using pixel_position = vec2<int>;

struct mask_row {
  uint8_t skip;
  uint8_t width;
};

// surfaces are disjoint, tightly packed 320-pixel rows. Clipping preserves source/destination correspondence
void copy_rectangle(std::span<uint8_t const> source, std::span<uint8_t> target,
  pixel_position const &source_origin, pixel_position const &destination, int width, int height);
void copy_mask(std::span<uint8_t const> source, std::span<uint8_t> target,
  pixel_position const &source_origin, pixel_position const &destination, std::span<mask_row const> rows);

} // namespace darker::graphics
