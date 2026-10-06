#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include "graphics/blit.h"
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

enum class craft { caero, skimma, upgraded_skimma };

struct hud_strip {
  int y_offset;
  std::span<mask_row const> rows;
};

struct hud_component {
  unsigned int address;
  unsigned int field;
  std::string_view label;
  pixel_position on_source;
  pixel_position destination;
  pixel_position alternate_source;
  std::span<hud_strip const> strips;
};

std::span<hud_component const> cockpit_components(craft type);
std::size_t instrument_limit(craft type, std::size_t component);
framework::render::indexed_cockpit_framebuffer make_cockpit_cache(framework::render::indexed_framebuffer const &sheet);
void draw_skimma_shield_startup(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target, std::uint8_t state);
void clear_windscreen(framework::render::indexed_cockpit_framebuffer &target, craft type, std::uint8_t colour);
void update_instrument(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target, craft type, std::size_t component,
  std::uint8_t old_state, std::uint8_t new_state);

} // namespace darker::graphics
