#pragma once

#include <cstdint>
#include <span>
#include "graphics/render_geometry.h"
#include "graphics/flat_polygon.h"

namespace darker::graphics {

struct shaded_vertex {
  vec2<render_geometry::screen_coordinate> position{};
  uint16_t shade{0};
};

void draw_gouraud_polygon(framework::render::indexed_cockpit_framebuffer &target,
  std::span<shaded_vertex const> vertices, int right = display_layout::right, int bottom = display_layout::height);

} // namespace darker::graphics
