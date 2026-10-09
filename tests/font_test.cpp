#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <vector>
#include "graphics/font.h"
#include "graphics/formatted_text.h"
#include "graphics/procedural_hud.h"
#include "reference/threat_samples.h"

namespace {

std::vector<std::byte> font_bytes() {
  /// One synthetic glyph has an edge pixel, an ink pixel and a transparent pixel in all four phase tables
  std::vector<std::byte> bytes(16000);
  bytes[107 + ('A' - 33) * 2] = std::byte{3};
  bytes[107 + ('A' - 33) * 2 + 1] = std::byte{1};
  for(unsigned int phase{0}; phase < 4; ++phase) {
    unsigned int const table{107 * (3 + phase * 2)};
    unsigned int const target{15900 + phase * 2};
    auto const offset{target - table};
    bytes[table + ('A' - 33) * 2] = static_cast<std::byte>(offset & 255);
    bytes[table + ('A' - 33) * 2 + 1] = static_cast<std::byte>(offset >> 8);
    bytes[target + phase / 4] |= static_cast<std::byte>(1 << (phase % 4));
    bytes[target + (phase + 1) / 4] |= static_cast<std::byte>(17 << ((phase + 1) % 4));
  }
  return bytes;
}

} // namespace

TEST_CASE("Aircraft warning glyphs use native brightness and strongest-left ordering", "[graphics][font]") {
  auto bytes{font_bytes()};
  unsigned int constexpr base{0x226a}, count{97}, index{0x81 - 33};
  bytes[base + count + index * 2] = std::byte{2};
  bytes[base + count + index * 2 + 1] = std::byte{1};
  auto const table{base + count * 3};
  unsigned int constexpr source{15800};
  bytes[table + index * 2] = static_cast<std::byte>((source - table) & 255);
  bytes[table + index * 2 + 1] = static_cast<std::byte>((source - table) >> 8);
  bytes[source] = std::byte{0x23}; // Edge then ink; remaining pixels untouched
  darker::resources::font_resource const font{std::move(bytes)};
  for(auto const &sample : darker::test_reference::threat_samples) {
    framework::render::indexed_cockpit_framebuffer frame;
    frame.pixels.fill(42);
    darker::graphics::draw_aircraft_threats(frame,font,sample.after);
    for(std::size_t i{0}; i < 4; ++i) {
      auto const offset{180 * 320 + 232 - i * 8};
      auto const level{sample.levels[i]};
      CHECK(frame.pixels[offset] == (level == 0 ? 42 : level == 1 ? 0 : level + 223));
      CHECK(frame.pixels[offset + 1] == (level == 0 ? 42 : level + 229));
      CHECK(frame.pixels[offset + 2] == 42);
    }
  }
}

TEST_CASE("Font drawing preserves transparency and clips at framebuffer edges", "[graphics][font]") {
  /// Negative positions retain their source phase while only visible coverage reaches the target
  darker::resources::font_resource const font{font_bytes()};
  for(std::int16_t const x : std::array<std::int16_t, 9>{-2, -1, 0, 1, 2, 3, 318, 319, 320}) {
    framework::render::indexed_cockpit_framebuffer frame;
    frame.pixels.fill(7);
    CHECK(darker::graphics::draw_glyph(frame, font, darker::resources::font_face::interface, 'A', {.x{x}, .y{239}}, {.ink{9}, .edge{4}}) == static_cast<std::uint16_t>(x + 3));
    for(int column{0}; column < 320; ++column) {
      CHECK(frame.pixels[239 * 320 + column] == (column == x ? 4 : column == x + 1 ? 9 : 7));
    }
    CHECK(frame.pixels[0] == 7);
  }
  CHECK_THROWS_AS(darker::resources::font_resource{std::vector<std::byte>(10)}, std::invalid_argument);
  CHECK_THROWS_AS(font.glyph(darker::resources::font_face::interface, 153), std::out_of_range);
}

TEST_CASE("Text formatter rejects truncated controls and stops at a page boundary", "[graphics][font]") {
  /// The caller retains ownership of subsequent pages and counted mission-message bytes
  darker::resources::font_resource const font{font_bytes()};
  std::array<std::byte, 3> const pages{std::byte{'A'}, std::byte{0}, std::byte{255}};
  auto const first{darker::graphics::lay_out_text(pages, font, darker::resources::font_face::interface)};
  REQUIRE(first.glyphs.size() == 1);
  CHECK(first.consumed == 2);
  CHECK(first.cursor.x == 3);
  for(unsigned int const code : {1, 2, 5, 6, 8, 31}) {
    std::array<std::byte, 1> const text{static_cast<std::byte>(code)};
    CHECK_THROWS_AS(darker::graphics::lay_out_text(text, font, darker::resources::font_face::interface), std::invalid_argument);
  }
}
