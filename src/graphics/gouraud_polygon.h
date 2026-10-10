#pragma once

#include <cstdint>
#include <span>
#include "graphics/flat_polygon.h"

namespace darker::graphics {

struct shaded_vertex {
  int16_t x{0};
  int16_t y{0};
  uint16_t shade{0};
};

void draw_gouraud_polygon(framework::render::indexed_cockpit_framebuffer &target,
  std::span<shaded_vertex const> vertices, int right = 319, int bottom = 240);

} // namespace darker::graphics
