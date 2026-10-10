#pragma once

#include <cstdint>
#include "graphics/blit.h"
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

void draw_screen_line(framework::render::indexed_cockpit_framebuffer &target, pixel_position const &first, pixel_position const &last, std::uint8_t colour);
bool clip_world_line(pixel_position &first, pixel_position &last, int bottom);
void draw_world_line(framework::render::indexed_cockpit_framebuffer &target, pixel_position const &first, pixel_position const &last, std::uint8_t colour, int bottom);
void draw_disc(framework::render::indexed_cockpit_framebuffer &target, pixel_position const &centre, unsigned int radius, std::uint8_t colour, int bottom);

} // namespace darker::graphics
