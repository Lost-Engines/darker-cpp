#pragma once

#include <array>
#include <cstdint>
#include "graphics/blit.h"
#include "graphics/font.h"
#include "graphics/screen_primitives.h"
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

enum class target_marker { small, large, skimma_aim };

void draw_aircraft_threats(framework::render::indexed_cockpit_framebuffer &target,
  resources::font_resource const &font, std::array<uint8_t, 4> const &errors);

void draw_missile_camera_indicator(framework::render::indexed_cockpit_framebuffer &target,
  uint16_t clock, bool enabled, bool following);

struct attitude_line {
  pixel_position first;
  pixel_position last;
  uint8_t colour;
};

attitude_line calculate_attitude(uint16_t pitch_index, uint16_t roll_index,
  int8_t pitch_high, bool alternate_colour, int centre_y = 92);
void draw_attitude_surround(framework::render::indexed_cockpit_framebuffer &target, uint16_t colour_parameter, int centre_y = 92);
void draw_target_marker(framework::render::indexed_cockpit_framebuffer &target,
  target_marker marker, pixel_position const &centre, uint8_t upper_colour, uint8_t lower_colour);

} // namespace darker::graphics
