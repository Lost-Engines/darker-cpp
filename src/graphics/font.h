#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include "graphics/blit.h"
#include "render/indexed_framebuffer.h"
#include "resources/font.h"

namespace darker::graphics {

struct text_colours {
  uint8_t ink{255};
  uint8_t edge{0};
};

uint16_t draw_glyph(framework::render::indexed_cockpit_framebuffer &target, resources::font_resource const &font,
  resources::font_face face, uint8_t code, pixel_position const &position, text_colours colours);

void draw_message(framework::render::indexed_cockpit_framebuffer &target, resources::font_resource const &font,
  resources::font_face face, std::span<std::byte const> text, pixel_position const &position, uint16_t width, text_colours colours);

uint16_t draw_text(framework::render::indexed_cockpit_framebuffer &target, resources::font_resource const &font,
  resources::font_face face, std::span<std::byte const> text, pixel_position const &position, text_colours colours);

} // namespace darker::graphics
