#include "graphics/font.h"
#include <algorithm>
#include <bit>

namespace darker::graphics {

std::uint16_t draw_glyph(framework::render::indexed_cockpit_framebuffer &target, resources::font_resource const &font,
  resources::font_face const face, std::uint8_t const code, pixel_position const &position, text_colours const colours) {
  /// Translate E1FE's planar coverage and foreground/shadow pattern to indexed pixels without antialiasing
  unsigned int const phase{static_cast<std::uint16_t>(position.x) & 3u};
  auto const glyph{font.glyph(face, code, phase)};
  for(unsigned int y{0}; y < glyph.height; ++y) {
    int const row{position.y + glyph.top + static_cast<int>(y)};
    if(row < 0 || row >= 240) continue;
    for(unsigned int x{0}; x < glyph.stride * 4; ++x) {
      int const column{position.x + static_cast<int>(x) - static_cast<int>(phase)};
      if(column < 0 || column >= 320) continue;
      auto const bits{std::to_integer<std::uint8_t>(glyph.planes[y * glyph.stride + x / 4])};
      auto const plane{x & 3};
      if(bits & (1 << plane)) target.pixels[static_cast<std::size_t>(row * 320 + column)] = bits & (16 << plane) ? colours.ink : colours.edge;
    }
  }
  return static_cast<std::uint16_t>(position.x + glyph.advance);
}

void draw_message(framework::render::indexed_cockpit_framebuffer &target, resources::font_resource const &font,
  resources::font_face const face, std::span<std::byte const> const text, pixel_position const &position,
  uint16_t const width, text_colours const colours) {
  /// B221 clears a nine-row backing strip with three-pixel side margins before drawing the counted glyphs
  if(width < 2) return;
  int const left{position.x-3};
  int const right{left+(width/2+3)*2};
  for(int y{std::max(0,position.y)}; y < std::min(240,position.y+9); ++y) {
    for(int x{std::max(0,left)}; x < std::min(320,right); ++x) target.pixels[static_cast<size_t>(y*320+x)] = 0;
  }
  draw_text(target,font,face,text,position,colours);
}

std::uint16_t draw_text(framework::render::indexed_cockpit_framebuffer &target, resources::font_resource const &font,
  resources::font_face const face, std::span<std::byte const> const text, pixel_position const &position, text_colours const colours) {
  /// Counted message payloads contain glyph codes; their length and timing belong to the script consumer
  auto cursor{position};
  for(auto const byte : text) cursor.x = std::bit_cast<std::int16_t>(draw_glyph(target, font, face, std::to_integer<std::uint8_t>(byte), cursor, colours));
  return static_cast<std::uint16_t>(cursor.x);
}

} // namespace darker::graphics
