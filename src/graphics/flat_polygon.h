#pragma once

#include <array>
#include <cstdint>
#include <span>
#include "graphics/render_geometry.h"
#include "vectorstorm/vector/vector2.h"
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

using screen_vertex = vec2<render_geometry::screen_coordinate>;

inline unsigned int constexpr polygon_vertex_limit{256};                       // model vertex indices are bytes
inline unsigned int constexpr clipped_polygon_vertex_limit{polygon_vertex_limit + 4}; // one extra vertex per viewport plane

struct viewport_clip_plane {
  bool horizontal;
  int boundary;
  bool maximum;
};

inline auto viewport_clip_planes(int const right, int const bottom) noexcept {
  /// Preserve the native bottom, top, right, left clipping order
  return std::array{
    viewport_clip_plane{
      .horizontal{false},
      .boundary{bottom},
      .maximum{true},
    },
    viewport_clip_plane{
      .horizontal{false},
      .boundary{0},
      .maximum{false},
    },
    viewport_clip_plane{
      .horizontal{true},
      .boundary{right},
      .maximum{true},
    },
    viewport_clip_plane{
      .horizontal{true},
      .boundary{0},
      .maximum{false},
    },
  };
}

bool back_facing(screen_vertex const &origin, screen_vertex const &next, screen_vertex const &previous) noexcept;

void draw_flat_polygon(framework::render::indexed_cockpit_framebuffer &target, std::span<screen_vertex const> vertices,
  uint8_t colour, int right = display_layout::right, int bottom = display_layout::height);

} // namespace darker::graphics
