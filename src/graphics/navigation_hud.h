#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include "graphics/blit.h"
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

struct world_position {
  std::uint16_t x;
  std::uint16_t y;
};

enum class radar_group { a, b, underground };

struct radar_contact {
  world_position position;
  radar_group group;
  bool hidden{false};
  bool covered{true};
};

enum class radar_scale { normal, enlarged };

struct radar_view_state {
  world_position player;
  std::uint16_t heading;
  std::uint8_t row;
  std::uint8_t column;
};

struct radar_pixel {
  pixel_position position;
  std::uint8_t colour;
};

std::uint8_t compass_phase(std::uint16_t heading) noexcept;
std::array<pixel_position, 5> compass_points(std::uint8_t phase);
void update_compass(framework::render::indexed_cockpit_framebuffer &target, std::uint8_t previous, std::uint8_t current);
std::optional<radar_pixel> project_radar_contact(world_position player, std::uint16_t heading, radar_contact contact, radar_scale scale = radar_scale::normal);
void draw_radar_contacts(framework::render::indexed_cockpit_framebuffer &target,
  world_position player, std::uint16_t heading, std::span<radar_contact const> contacts);

void draw_navigation_contact(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target, pixel_position destination,
  std::uint8_t height, std::uint8_t reference_height);
void draw_enlarged_radar_surround(framework::render::indexed_cockpit_framebuffer &target, std::uint16_t heading);
void draw_enlarged_radar(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target, radar_view_state view, std::span<radar_contact const> contacts);

} // namespace darker::graphics
