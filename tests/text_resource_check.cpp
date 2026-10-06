#include "text_resource_check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>
#include "graphics/font.h"
#include "graphics/formatted_text.h"
#include "reference/font_samples.h"
#include "reference/text_samples.h"
#include "resources/archive_set.h"

void check_text_resources(darker::resources::archive_set const &archives) {
  /// Compare native glyph paths and formatter state independently of final palette selection
  darker::resources::font_resource const fonts{archives.load({.archive{0}, .slot{29}})};
  for(auto const &sample : darker::test_reference::font_samples) {
    std::uint64_t fingerprint{0xcbf29ce484222325};
    auto const add{[&](std::uint32_t const value){
      for(unsigned int shift{0}; shift < 32; shift += 8) fingerprint = (fingerprint ^ ((value >> shift) & 255)) * 0x100000001b3;
    }};
    for(unsigned int code{32}; code < 33 + sample.count; ++code) {
      framework::render::indexed_cockpit_framebuffer frame{};
      auto const cursor{darker::graphics::draw_glyph(frame, fonts, static_cast<darker::resources::font_face>(sample.face),
        static_cast<std::uint8_t>(code), {.x{static_cast<std::int16_t>(8 + sample.phase)}, .y{9}}, {.ink{2}, .edge{1}})};
      add(cursor);
      std::uint32_t count{0};
      for(auto const pixel : frame.pixels) count += pixel != 0;
      add(count);
      for(std::size_t i{0}; i < frame.pixels.size(); ++i) {
        if(!frame.pixels[i]) continue;
        add(static_cast<std::uint32_t>(i));
        add(frame.pixels[i]);
      }
    }
    if(fingerprint != sample.fingerprint) throw std::runtime_error{std::format("Font {} alignment {} differs from native drawing", sample.face, sample.phase)};
  }
  std::cout << "All 300 glyphs and space advances match native coverage and two-colour patterns at all four alignments." << std::endl;
  std::array<std::vector<std::byte>, 16> resources;
  for(unsigned int slot{0}; slot < resources.size(); ++slot) resources[slot] = archives.load({.archive{4}, .slot{slot}});
  std::array<std::span<std::uint8_t const>, 3> const extra{
    darker::test_reference::text_extra_0, darker::test_reference::text_extra_1, darker::test_reference::text_extra_2,
  };
  for(auto const &sample : darker::test_reference::text_samples) {
    std::span<std::byte const> const bytes{sample.resource == 16 ? std::as_bytes(extra.at(sample.offset))
      : std::span<std::byte const>{resources.at(sample.resource)}.subspan(sample.offset, sample.size)};
    darker::graphics::formatted_page page;
    try {
      page = darker::graphics::lay_out_text(bytes, fonts, darker::resources::font_face::interface, {.colour{0x3456}, .runtime_number{195}});
      if(!sample.supported) throw std::runtime_error{"Invalid source glyph was silently accepted"};
    } catch(std::out_of_range const&) {
      if(!sample.supported) continue;
      throw;
    }
    std::uint64_t fingerprint{0xcbf29ce484222325};
    auto const add{[&](std::uint32_t const value){
      for(unsigned int shift{0}; shift < 32; shift += 8) fingerprint = (fingerprint ^ ((value >> shift) & 255)) * 0x100000001b3;
    }};
    add(static_cast<std::uint32_t>(page.consumed));
    add(page.cursor.x);
    add(page.cursor.y);
    add(page.cursor.colour);
    add(page.cursor.margin);
    add(static_cast<std::uint8_t>(page.cursor.line_step));
    add(static_cast<std::uint32_t>(page.glyphs.size()));
    for(auto const glyph : page.glyphs) {
      add(glyph.code);
      add(static_cast<std::uint16_t>(glyph.position.x));
      add(static_cast<std::uint16_t>(glyph.position.y));
      add(glyph.colour);
    }
    if(fingerprint != sample.fingerprint) throw std::runtime_error{std::format("Formatted text resource {}, offset {} differs from native layout", sample.resource, sample.offset)};
  }
  std::cout << "544 original pages and three additional control cases match native layout; two original out-of-font glyph pages are explicitly rejected." << std::endl;
}
