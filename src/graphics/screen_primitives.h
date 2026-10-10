#pragma once

#include <cstdint>
#include "graphics/blit.h"
#include "graphics/raster_viewport.h"
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

void draw_screen_line(framework::render::indexed_surface target, pixel_position const &first, pixel_position const &last, uint8_t colour);
bool clip_world_line(pixel_position &first, pixel_position &last, raster_viewport viewport);
void draw_world_line(framework::render::indexed_surface target, pixel_position const &first, pixel_position const &last, uint8_t colour, raster_viewport viewport);
void draw_disc(framework::render::indexed_surface target, pixel_position const &centre, unsigned int radius, uint8_t colour, raster_viewport viewport);

} // namespace darker::graphics
