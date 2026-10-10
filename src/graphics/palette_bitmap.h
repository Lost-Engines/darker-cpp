#pragma once

#include <bitset>
#include <cstddef>
#include <span>
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

struct palette_state {
  framework::render::colour_palette colours{};
  std::bitset<256> defined;
};

struct palette_update {
  palette_state palette;
  size_t bytes_consumed;
};

struct palette_bitmap {
  palette_state palette;
  framework::render::indexed_framebuffer image;
};

uint8_t palette_fade_gain(uint16_t phase) noexcept;
uint8_t palette_dac_component(uint8_t component, uint8_t gain) noexcept;
framework::render::colour_palette fade_palette(framework::render::colour_palette const &colours, uint16_t phase) noexcept;

palette_update decode_palette(std::span<std::byte const> data, palette_state previous = {});
palette_bitmap decode_bitmap(std::span<std::byte const> data, palette_state previous = {});

} // namespace darker::graphics
