#pragma once

#include <array>
#include <cstdint>

namespace framework::render {

struct rgba_pixel {
  std::uint8_t red;
  std::uint8_t green;
  std::uint8_t blue;
  std::uint8_t alpha{255};
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
