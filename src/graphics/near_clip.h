#pragma once

#include <cstdint>
#include <span>
#include "graphics/gouraud_polygon.h"

namespace darker::graphics {

struct camera_vertex {
  std::int32_t horizontal{0};                                                   // signed 24-bit values, with eight fractional bits
  std::int32_t vertical{0};
  std::int32_t depth{0};
};

screen_vertex project_vertex(camera_vertex vertex, screen_vertex origin);
screen_vertex near_intersection(camera_vertex inside, camera_vertex outside, screen_vertex origin);
std::size_t clip_near_polygon(std::span<camera_vertex const> vertices, screen_vertex origin, std::span<screen_vertex> output);

std::size_t clip_near_shaded_polygon(std::span<camera_vertex const> vertices, std::span<std::uint16_t const> shades,
  screen_vertex origin, std::span<shaded_vertex> output);

} // namespace darker::graphics
