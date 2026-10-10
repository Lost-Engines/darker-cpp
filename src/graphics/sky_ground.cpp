#include "graphics/sky_ground.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include "graphics/screen_layout.h"
#include "maths/angle.h"

namespace darker::graphics {

void draw_sky_ground(framework::render::indexed_surface target, camera_angles const angles, screen_vertex const &origin, int const bottom) {
  /// Reconstruct B409's indexed sky/ground bands using its rounded angles, integer projection and B71A colour sequence
  if(bottom < 0 || bottom > target.height) throw std::invalid_argument{"Sky/ground viewport exceeds the framebuffer"};
  std::array<uint8_t, 17> constexpr colours{28, 29, 30, 31, 60, 61, 62, 63, 252, 92, 93, 94, 95, 124, 125, 126, 127};
  std::array<int, 16> constexpr offsets{74, 56, 42, 30, 20, 12, 6, 2, -2, -6, -12, -20, -30, -42, -56, -74};
  auto pitch{maths::view_angle_phase(angles.pitch)};
  auto roll{maths::view_angle_phase(angles.roll)};
  if(((pitch >> 7) & 3) == 1 || ((pitch >> 7) & 3) == 2) {
    for(int y{0}; y < bottom; ++y) std::ranges::fill(target.row(y), pitch < 512 ? colours.front() : colours.back());
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
  auto const cosine{maths::phase_cosine(roll)};
  auto const tangent{maths::original_sine[pitch] * 512 / maths::phase_cosine(pitch)};
  int const centre_x{origin.x + ((tangent * sine) >> 16)};
  int const centre_y{origin.y + ((tangent * cosine) >> 16)};
  std::array<int, 16> boundaries{};
  // preserve the original independent signed high-word products at each band boundary
  int const slope{sine == 0 ? 0 : cosine * 256 / sine};
  if((sine == 0 || slope >= 64 * 256) && (centre_y + 41 <= 0 || (sine != 0 && centre_y - 41 >= bottom))) {
    for(int y{0}; y < bottom; ++y) std::ranges::fill(target.row(y), outer_colour);
    return;
  }
  int const first_row{sine != 0 && slope >= 64 * 256 && centre_y + 41 >= bottom ? std::max(0, centre_y - 41) : 0};
  for(size_t i{0}; i < offsets.size(); ++i) {
    int const x{centre_x - ((offsets[i] * sine) >> 16)};
    int const y{centre_y - first_row - ((offsets[i] * cosine) >> 16)};
    boundaries[i] = sine == 0 ? y : x + ((y * slope) >> 8);
  }
  for(int row{0}; row < bottom; ++row) {
    int const y{mirror ? bottom - 1 - row : row};
    if(row < first_row) {
      std::fill_n(target.pixels.begin() + y * target.stride, target.width, outer_colour);
      continue;
    }
    // Along a scanline the band coordinate increases by exactly one per pixel.
    // Find the first band once, then fill up to each boundary instead of doing
    // a binary search for every pixel. A level horizon has one colour per row.
    int const coordinate{sine == 0 ? row : ((row - first_row) * slope + 255) >> 8};
    auto band{static_cast<unsigned int>(std::upper_bound(boundaries.begin(), boundaries.end(), coordinate) - boundaries.begin())};
    auto pixels{target.row(y)};
    if(sine == 0) {
      std::ranges::fill(pixels, colours[reverse ? 16 - band : band]);
      continue;
    }
    int x{0};
    while(x < target.width) {
      int const end{band == boundaries.size() ? target.width : std::min(target.width, boundaries[band] - coordinate)};
      std::fill(pixels.begin() + x, pixels.begin() + end, colours[reverse ? 16 - band : band]);
      x = end;
      ++band;
    }
  }
}

} // namespace darker::graphics
