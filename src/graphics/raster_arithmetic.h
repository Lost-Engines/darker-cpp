#pragma once

#include <bit>
#include <cstdint>

namespace darker::graphics {

// Raster interpolation precision is independent of retained camera-space fractions.
struct raster_arithmetic {
  using division_quotient = int16_t;                                          // native IDIV result, shared by clipping coordinates and shades
  using palette_accumulator = uint16_t;
  using palette_difference = int16_t;
  static unsigned int constexpr edge_fraction_bits{8};
  static int constexpr edge_fraction_scale{1 << edge_fraction_bits};
  static int constexpr edge_fraction_mask{edge_fraction_scale - 1};
  static int constexpr edge_start_fraction{edge_fraction_scale / 2};

  static constexpr palette_difference wrap_palette(int value) noexcept {
    /// Palette ramps retain word differences regardless of screen-coordinate precision
    return std::bit_cast<palette_difference>(static_cast<palette_accumulator>(value));
  }
};

} // namespace darker::graphics
