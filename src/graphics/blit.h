#pragma once

#include <cstdint>
#include <span>
#include "vectorstorm/vector/vector2.h"
#include "render/indexed_surface.h"

namespace darker::graphics {

using pixel_position = vec2<int>;

struct mask_row {
  uint8_t skip;
  uint8_t width;
};

// Surfaces are disjoint; clipping preserves source/destination correspondence, irrespective of their strides.
void copy_rectangle(framework::render::const_indexed_surface source, framework::render::indexed_surface target,
  pixel_position const &source_origin, pixel_position const &destination, int width, int height);
void copy_mask(framework::render::const_indexed_surface source, framework::render::indexed_surface target,
  pixel_position const &source_origin, pixel_position const &destination, std::span<mask_row const> rows);

} // namespace darker::graphics
