#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include "graphics/blit.h"
#include "resources/font.h"

namespace darker::graphics {

struct text_cursor {
  uint16_t x{0};
  uint16_t y{0};
  uint16_t colour{0};
  uint8_t margin{0};
  int8_t line_step{16};
  uint8_t runtime_number{0};
};

struct positioned_glyph {
  pixel_position position{};
  uint16_t colour{0};
  uint8_t code{0};
};

struct formatted_page {
  std::vector<positioned_glyph> glyphs{};
  size_t consumed{0};
  text_cursor cursor{};
};

formatted_page lay_out_text(std::span<std::byte const> text, resources::font_resource const &font,
  resources::font_face face, text_cursor cursor = {});

} // namespace darker::graphics
