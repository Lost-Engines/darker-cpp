#pragma once

#include <cstdint>
#include <span>
#include "vectorstorm/vector/vector2.h"
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

using screen_vertex = vector2<std::int16_t>;

bool back_facing(screen_vertex const &origin, screen_vertex const &next, screen_vertex const &previous) noexcept;

void draw_flat_polygon(framework::render::indexed_cockpit_framebuffer &target, std::span<screen_vertex const> vertices,
  std::uint8_t colour, int right = 319, int bottom = 240);

} // namespace darker::graphics
