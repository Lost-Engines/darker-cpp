#pragma once

#include <array>
#include <cstdint>
#include "render/framebuffer.h"

namespace framework::render {

using colour_palette = std::array<rgba_pixel, 256>;

struct indexed_framebuffer {
  std::array<std::uint8_t, framebuffer::width * framebuffer::height> pixels;
};

void expand_palette(indexed_framebuffer const &source, colour_palette const &palette, framebuffer &target) noexcept;

} // namespace framework::render
