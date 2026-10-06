#include "framebuffer.h"
#include <algorithm>
#include <cmath>

namespace framework::render {

viewport fit_viewport(int const width, int const height) noexcept {
  /// Fit the source aspect ratio, using integer enlargement whenever the window is large enough
  if(width <= 0 || height <= 0) return {.x{0}, .y{0}, .width{0}, .height{0}};
  double const available{std::min(width / static_cast<double>(framebuffer::width), height / static_cast<double>(framebuffer::height))};
  double const scale{available >= 1.0 ? std::floor(available) : available};
  int const fitted_width{std::max(1, static_cast<int>(framebuffer::width * scale))};
  int const fitted_height{std::max(1, static_cast<int>(framebuffer::height * scale))};
  return {
    .x{(width - fitted_width) / 2},
    .y{(height - fitted_height) / 2},
    .width{fitted_width},
    .height{fitted_height},
  };
}

void draw_demo(framebuffer &target, double const seconds) noexcept {
  /// Draw a CPU-only checkerboard and moving rectangle; the presenter only uploads the result
  unsigned int const left{static_cast<unsigned int>(120.0 + 90.0 * std::sin(seconds))};
  for(unsigned int y{0}; y != framebuffer::height; ++y) {
    for(unsigned int x{0}; x != framebuffer::width; ++x) {
      bool const light{((x / 16 + y / 16) % 2) != 0};
      target.pixels[y * framebuffer::width + x] = {
        .red{static_cast<std::uint8_t>(light ? 24 : 16)},
        .green{static_cast<std::uint8_t>(light ? 36 : 26)},
        .blue{static_cast<std::uint8_t>(light ? 52 : 40)},
      };
      if(x >= left && x < left + 80 && y >= 70 && y < 130) {
        target.pixels[y * framebuffer::width + x] = {.red{64}, .green{210}, .blue{170}};
      }
    }
  }
}

} // namespace framework::render
