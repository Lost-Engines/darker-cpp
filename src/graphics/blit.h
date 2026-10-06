#pragma once

#include <cstdint>
#include <span>

namespace darker::graphics {

struct pixel_position {
  int x;
  int y;
};

struct mask_row {
  std::uint8_t skip;
  std::uint8_t width;
};

// Surfaces are disjoint, tightly packed 320-pixel rows. Clipping preserves source/destination correspondence.
void copy_rectangle(std::span<std::uint8_t const> source, std::span<std::uint8_t> target,
  pixel_position source_origin, pixel_position destination, int width, int height);
void copy_mask(std::span<std::uint8_t const> source, std::span<std::uint8_t> target,
  pixel_position source_origin, pixel_position destination, std::span<mask_row const> rows);

} // namespace darker::graphics
