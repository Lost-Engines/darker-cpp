#pragma once

#include <cstdint>
#include "graphics/blit.h"
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

enum class target_marker { small, large, skimma_aim };

struct attitude_line {
  pixel_position first;
  pixel_position last;
  std::uint8_t colour;
};

attitude_line calculate_attitude(std::uint16_t pitch_index, std::uint16_t roll_index,
  std::int8_t pitch_high, bool alternate_colour);
void draw_hud_line(framework::render::indexed_cockpit_framebuffer &target,
  pixel_position first, pixel_position last, std::uint8_t colour);
void draw_attitude_surround(framework::render::indexed_cockpit_framebuffer &target, std::uint16_t colour_parameter);
void draw_target_marker(framework::render::indexed_cockpit_framebuffer &target,
  target_marker marker, pixel_position centre, std::uint8_t upper_colour, std::uint8_t lower_colour);

} // namespace darker::graphics
