#include "framebuffer.h"
#include <algorithm>
#include <cmath>

namespace framework::render {

viewport fit_viewport(int const width, int const height, int const source_width, int const source_height) noexcept {
  /// Fit the source aspect ratio, using integer enlargement whenever the window is large enough
  if(width <= 0 || height <= 0 || source_width <= 0 || source_height <= 0) return {.x{0}, .y{0}, .width{0}, .height{0}};
  double const available{std::min(width / static_cast<double>(source_width), height / static_cast<double>(source_height))};
  double const scale{available >= 1.0 ? std::floor(available) : available};
  int const fitted_width{std::max(1, static_cast<int>(source_width * scale))};
  int const fitted_height{std::max(1, static_cast<int>(source_height * scale))};
  return {
    .x{(width - fitted_width) / 2},
    .y{(height - fitted_height) / 2},
    .width{fitted_width},
    .height{fitted_height},
  };
}

} // namespace framework::render
