#pragma once

#include <array>
#include <cstdint>
#include <span>
#include "vectorstorm/vector/vector2.h"
#include "graphics/raster_viewport.h"
#include "graphics/render_geometry.h"
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

using screen_vertex = vec2<render_geometry::screen_coordinate>;

inline unsigned int constexpr polygon_vertex_limit{256};                       // model vertex indices are bytes
inline unsigned int constexpr clipped_polygon_vertex_limit{polygon_vertex_limit + 4}; // one extra vertex per viewport plane

bool back_facing(screen_vertex const &origin, screen_vertex const &next, screen_vertex const &previous) noexcept;

void draw_flat_polygon(framework::render::indexed_surface target, std::span<screen_vertex const> vertices,
  uint8_t colour, raster_viewport viewport = {});

} // namespace darker::graphics
