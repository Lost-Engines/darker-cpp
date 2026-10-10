#pragma once

#include <cstdint>
#include <span>
#include "graphics/flat_polygon.h"
#include "graphics/raster_arithmetic.h"
#include "graphics/render_geometry.h"

namespace darker::graphics {

struct shaded_vertex {
  vec2<render_geometry::screen_coordinate> position{};
  raster_arithmetic::palette_accumulator shade{0};
};

void draw_gouraud_polygon(framework::render::indexed_surface target,
  std::span<shaded_vertex const> vertices, raster_viewport viewport = {});

} // namespace darker::graphics
