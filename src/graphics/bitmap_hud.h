#pragma once

#include <array>
#include <cstdint>
#include "graphics/blit.h"
#include "graphics/cockpit.h"
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

enum class coordinate_font { small, large };

enum class weapon_icon_slot { primary, secondary };

struct caero_bitmap_state {
  uint8_t row{0};
  uint8_t column{0};
  uint8_t primary_weapon{0};
  uint8_t secondary_weapon{0};
};

struct skimma_bitmap_state {
  uint8_t bearing{0};
  std::array<uint8_t, 3> weapons{};
};

void update_skimma_bitmaps(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target, craft type, skimma_bitmap_state previous, skimma_bitmap_state current);

void draw_skimma_weapon_ring(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target, craft type,
  uint8_t weapon, uint8_t radius, uint8_t remaining, int baseline_y = 88);

void draw_weapon_icon(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target, weapon_icon_slot slot, uint8_t selection);
void draw_grid_coordinate(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target, pixel_position const &destination, uint8_t encoded_coordinate, coordinate_font font = coordinate_font::small);
void update_caero_bitmaps(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target, caero_bitmap_state previous, caero_bitmap_state current);

} // namespace darker::graphics
