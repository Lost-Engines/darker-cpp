#pragma once

#include <array>
#include "graphics/screen_layout.h"

namespace darker::graphics {

// Native zero-origin viewport: right is an included pixel, bottom is an excluded raster row.
// Clipped vertices may lie on bottom; edge walkers stop before drawing that row.
struct raster_viewport {
  int right{display_layout::right};
  int bottom{display_layout::height};

  constexpr bool fits(int width, int height) const noexcept {
    /// Preserve the original non-empty viewport admission rules
    return right >= 0 && right < width && bottom > 0 && bottom <= height;
  }

  constexpr bool contains(int x, int y) const noexcept {
    /// Point primitives use pixel bounds, not the permitted polygon endpoint boundary
    return x >= 0 && x <= right && y >= 0 && y < bottom;
  }
};

struct viewport_clip_plane {
  bool horizontal;
  int boundary;
  bool maximum;
};

inline auto viewport_clip_planes(raster_viewport const viewport) noexcept {
  /// Preserve the native bottom, top, right, left clipping order and endpoint anchoring
  return std::array{
    viewport_clip_plane{
      .horizontal{false},
      .boundary{viewport.bottom},
      .maximum{true},
    },
    viewport_clip_plane{
      .horizontal{false},
      .boundary{0},
      .maximum{false},
    },
    viewport_clip_plane{
      .horizontal{true},
      .boundary{viewport.right},
      .maximum{true},
    },
    viewport_clip_plane{
      .horizontal{true},
      .boundary{0},
      .maximum{false},
    },
  };
}

} // namespace darker::graphics
