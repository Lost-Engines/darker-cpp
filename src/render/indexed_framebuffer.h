#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "render/framebuffer.h"

namespace framework::render {

using colour_palette = std::array<rgba_pixel, 256>;

template<unsigned int rows>
struct basic_indexed_framebuffer {
  static unsigned int constexpr width{320};
  static unsigned int constexpr height{rows};
  std::array<uint8_t, width * height> pixels;
};

using indexed_framebuffer = basic_indexed_framebuffer<200>;
using indexed_cockpit_framebuffer = basic_indexed_framebuffer<240>;

template<unsigned int rows>
void expand_palette(basic_indexed_framebuffer<rows> const &source, colour_palette const &palette, basic_framebuffer<rows> &target) noexcept {
  /// Resolve indices only at the presentation boundary
  for(size_t i{0}; i < source.pixels.size(); ++i) target.pixels[i] = palette[source.pixels[i]];
}

} // namespace framework::render
