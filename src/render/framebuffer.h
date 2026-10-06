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

struct framebuffer {
  static unsigned int constexpr width{320};
  static unsigned int constexpr height{200};
  std::array<rgba_pixel, width * height> pixels;
};

struct viewport {
  int x;
  int y;
  int width;
  int height;
};

viewport fit_viewport(int width, int height) noexcept;
void draw_demo(framebuffer &target, double seconds) noexcept;

} // namespace framework::render
