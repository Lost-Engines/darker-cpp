#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "render/frame_layout.h"
#include "render/framebuffer.h"
#include "render/indexed_surface.h"

namespace framework::render {

using colour_palette = std::array<rgba_pixel, 256>;

template<unsigned int rows, unsigned int columns = source_sheet_layout::width>
struct basic_indexed_framebuffer {
  static unsigned int constexpr width{columns};
  static unsigned int constexpr height{rows};
  std::array<uint8_t, width * height> pixels;
};

using indexed_framebuffer = basic_indexed_framebuffer<source_sheet_layout::height, source_sheet_layout::width>;
using indexed_cockpit_framebuffer = basic_indexed_framebuffer<display_layout::height, display_layout::width>;

template<unsigned int rows, unsigned int columns>
void expand_palette(basic_indexed_framebuffer<rows, columns> const &source, colour_palette const &palette, basic_framebuffer<rows, columns> &target) noexcept {
  /// Resolve indices only at the presentation boundary
  for(size_t i{0}; i < source.pixels.size(); ++i) target.pixels[i] = palette[source.pixels[i]];
}

} // namespace framework::render
