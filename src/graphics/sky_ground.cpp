#include "graphics/sky_ground.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include "maths/sine_table.h"

namespace darker::graphics {

void draw_sky_ground(framework::render::indexed_cockpit_framebuffer &target, camera_angles const angles, screen_vertex const &origin, int const bottom) {
  /// Reconstruct B409's indexed sky/ground bands using its rounded angles, integer projection and B71A colour sequence
  if(bottom < 0 || bottom > 240) throw std::invalid_argument{"Sky/ground viewport exceeds the framebuffer"};
  std::array<std::uint8_t, 17> constexpr colours{28, 29, 30, 31, 60, 61, 62, 63, 252, 92, 93, 94, 95, 124, 125, 126, 127};
  std::array<int, 16> constexpr offsets{74, 56, 42, 30, 20, 12, 6, 2, -2, -6, -12, -20, -30, -42, -56, -74};
  auto pitch{static_cast<std::uint16_t>(angles.pitch + 15) >> 6};
  auto roll{static_cast<std::uint16_t>(angles.roll + 15) >> 6};
  if(((pitch >> 7) & 3) == 1 || ((pitch >> 7) & 3) == 2) {
    std::fill_n(target.pixels.begin(), bottom * 320, pitch < 512 ? colours.front() : colours.back());
    return;
  }
  auto const outer_colour{pitch < 512 ? colours.front() : colours.back()};
  bool const reverse{roll >= 512};
  if(reverse) {
    roll &= 511;
    pitch ^= 511;
  }
  bool const mirror{roll >= 256};
  if(mirror) roll = 512 - roll;
  auto const sine{maths::original_sine[roll]};
  auto const cosine{maths::original_sine[(roll + 256) % 1024]};
  auto const tangent{maths::original_sine[pitch] * 512 / maths::original_sine[(pitch + 256) % 1024]};
  int const centre_x{origin.x + ((tangent * sine) >> 16)};
  int const centre_y{origin.y + ((tangent * cosine) >> 16)};
  std::array<int, 16> boundaries{};
  // preserve the original independent signed high-word products at each band boundary
  int const slope{sine == 0 ? 0 : cosine * 256 / sine};
  if((sine == 0 || slope >= 64 * 256) && (centre_y + 41 <= 0 || (sine != 0 && centre_y - 41 >= bottom))) {
    std::fill_n(target.pixels.begin(), bottom * 320, outer_colour);
    return;
  }
  int const first_row{sine != 0 && slope >= 64 * 256 && centre_y + 41 >= bottom ? std::max(0, centre_y - 41) : 0};
  for(std::size_t i{0}; i < offsets.size(); ++i) {
    int const x{centre_x - ((offsets[i] * sine) >> 16)};
    int const y{centre_y - first_row - ((offsets[i] * cosine) >> 16)};
    boundaries[i] = sine == 0 ? y : x + ((y * slope) >> 8);
  }
  for(int row{0}; row < bottom; ++row) {
    int const y{mirror ? bottom - 1 - row : row};
    if(row < first_row) {
      std::fill_n(target.pixels.begin() + y * 320, 320, outer_colour);
      continue;
    }
    for(int x{0}; x < 320; ++x) {
      int const coordinate{sine == 0 ? row : x + (((row - first_row) * slope + 255) >> 8)};
      auto const band{static_cast<std::size_t>(std::upper_bound(boundaries.begin(), boundaries.end(), coordinate) - boundaries.begin())};
      target.pixels[y * 320 + x] = colours[reverse ? 16 - band : band];
    }
  }
}

} // namespace darker::graphics
