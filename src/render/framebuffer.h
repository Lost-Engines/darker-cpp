#pragma once

#include <array>
#include <cstdint>
#include "render/frame_layout.h"
#include "vectorstorm/vector/vector4.h"

namespace framework::render {

// RGBA byte layout consumed directly by the framebuffer upload; pixels default to opaque
struct rgba_pixel : vec4<uint8_t> {
  constexpr rgba_pixel(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0, uint8_t alpha = 255) noexcept
    : vec4<uint8_t>{red, green, blue, alpha} {
  }
  explicit constexpr rgba_pixel(rgba_pixel const&) = default;
  constexpr rgba_pixel(rgba_pixel &&) = default;
  constexpr rgba_pixel &operator=(rgba_pixel const&) = default;
  constexpr rgba_pixel &operator=(rgba_pixel &&) = default;
};

static_assert(sizeof(rgba_pixel) == 4);

template<unsigned int rows, unsigned int columns = source_sheet_layout::width>
struct basic_framebuffer {
  static unsigned int constexpr width{columns};
  static unsigned int constexpr height{rows};
  std::array<rgba_pixel, width * height> pixels;
};

using framebuffer = basic_framebuffer<source_sheet_layout::height, source_sheet_layout::width>;
using cockpit_framebuffer = basic_framebuffer<display_layout::height, display_layout::width>;

struct viewport {
  int x;
  int y;
  int width;
  int height;
};

viewport fit_viewport(int width, int height, int source_width = source_sheet_layout::width, int source_height = source_sheet_layout::height) noexcept;

} // namespace framework::render
