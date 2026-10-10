#pragma once

#include <array>
#include <cstdint>
#include "vectorstorm/vector/vector4.h"

namespace framework::render {

// RGBA byte layout consumed directly by the framebuffer upload; pixels default to opaque.
struct rgba_pixel : vec4<std::uint8_t> {
  constexpr rgba_pixel(std::uint8_t red = 0, std::uint8_t green = 0, std::uint8_t blue = 0, std::uint8_t alpha = 255) noexcept
    : vec4<std::uint8_t>{red, green, blue, alpha} {}
  explicit constexpr rgba_pixel(rgba_pixel const &) = default;
  constexpr rgba_pixel(rgba_pixel &&) = default;
  constexpr rgba_pixel &operator=(rgba_pixel const &) = default;
  constexpr rgba_pixel &operator=(rgba_pixel &&) = default;
};

static_assert(sizeof(rgba_pixel) == 4);

template<unsigned int rows>
struct basic_framebuffer {
  static unsigned int constexpr width{320};
  static unsigned int constexpr height{rows};
  std::array<rgba_pixel, width * height> pixels;
};

using framebuffer = basic_framebuffer<200>;
using cockpit_framebuffer = basic_framebuffer<240>;

struct viewport {
  int x;
  int y;
  int width;
  int height;
};

viewport fit_viewport(int width, int height, int source_width = 320, int source_height = 200) noexcept;

} // namespace framework::render
