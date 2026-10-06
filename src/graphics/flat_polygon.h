#pragma once

#include <cstdint>
#include <span>
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

struct screen_vertex {
  std::int16_t x{0};
  std::int16_t y{0};
};

void draw_flat_polygon(framework::render::indexed_cockpit_framebuffer &target, std::span<screen_vertex const> vertices,
  std::uint8_t colour, int right = 319, int bottom = 240);

} // namespace darker::graphics
