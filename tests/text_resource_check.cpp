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
#include "presentation/menu_text.h"
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
    std::vector<unsigned int> codes;
    for(unsigned int code{32}; code < 33 + sample.count; ++code) codes.push_back(code);
    if(sample.face == 0) { codes.push_back(151); codes.push_back(153); }
    for(auto const code : codes) {
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
  std::cout << "All 300 glyphs, two German out-of-directory glyphs and space advances match native coverage and two-colour patterns at all four alignments." << std::endl;
  // The wide-font lookup for German level 76 leaves the resource. It must remain drawable at every alignment.
  for(unsigned int phase{0}; phase < 4; ++phase) {
    framework::render::indexed_cockpit_framebuffer actual{}, expected{};
    auto const position{darker::graphics::pixel_position{.x{static_cast<int16_t>(8+phase)}, .y{9}}};
    auto const advance{darker::graphics::draw_glyph(actual,fonts,darker::resources::font_face::wide,153,position,{.ink{2},.edge{1}})};
    auto const replacement{darker::graphics::draw_glyph(expected,fonts,darker::resources::font_face::wide,'?',position,{.ink{2},.edge{1}})};
    if(advance != replacement || actual.pixels != expected.pixels) throw std::runtime_error{"Undefined German glyph does not use a stable replacement"};
  }
  // This exception must not hide a damaged bitmap belonging to a declared glyph.
  auto truncated{archives.load({0,29})};
  truncated.resize(truncated.size()/2);
  bool rejected{false};
  try { darker::resources::font_resource const invalid{std::move(truncated)}; }
  catch(std::invalid_argument const &) { rejected = true; }
  if(!rejected) throw std::runtime_error{"Truncated font was accepted"};
  std::array<std::vector<std::byte>, 16> resources;
  for(unsigned int slot{0}; slot < resources.size(); ++slot) resources[slot] = archives.load({.archive{4}, .slot{slot}});
  std::array<std::span<std::uint8_t const>, 6> const extra{
    darker::test_reference::text_extra_0, darker::test_reference::text_extra_1, darker::test_reference::text_extra_2,
    darker::test_reference::text_extra_3, darker::test_reference::text_extra_4, darker::test_reference::text_extra_5,
  };
  for(auto const &sample : darker::test_reference::text_samples) {
    std::span<std::byte const> const bytes{sample.resource == 16 ? (sample.offset < 3 ? std::as_bytes(extra.at(sample.offset))
        : std::as_bytes(std::span{darker::presentation::original_credits.at(sample.offset-3)}))
      : std::span<std::byte const>{resources.at(sample.resource)}.subspan(sample.offset, sample.size)};
    auto const page{darker::graphics::lay_out_text(bytes, fonts, darker::resources::font_face::interface, {.colour{0x3456}, .runtime_number{195}})};
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
  std::cout << "546 original pages and three additional control cases and all three translated credits match native layout, including the two German out-of-directory glyphs." << std::endl;
}
