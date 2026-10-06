#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include "graphics/bitmap_hud.h"

namespace {

framework::render::indexed_cockpit_framebuffer digit_cache() {
  /// Distinct glyph colours expose digit choice, placement and restoration independently of artwork
  framework::render::indexed_cockpit_framebuffer cache;
  cache.pixels.fill(42);
  for(unsigned int digit{0}; digit < 10; ++digit) {
    for(unsigned int row{0}; row < 5; ++row) std::fill_n(cache.pixels.begin() + (8 + digit * 5 + row) * 320 + 308, 4, static_cast<std::uint8_t>(digit));
  }
  return cache;
}

} // namespace

TEST_CASE("Grid coordinate glyphs match native signed-byte and divide-by-nine boundaries") {
  auto const cache{digit_cache()};
  struct sample {
    std::uint8_t encoded;
    std::uint8_t tens;
    std::uint8_t units;
  };
  // Captured from native 5429/5455; signed high-bit inputs take the same restoration path as zero.
  std::array<sample, 10> const samples{{
    {.encoded{0}, .tens{42}, .units{42}},
    {.encoded{1}, .tens{0}, .units{1}},
    {.encoded{9}, .tens{0}, .units{1}},
    {.encoded{10}, .tens{0}, .units{2}},
    {.encoded{81}, .tens{0}, .units{9}},
    {.encoded{82}, .tens{1}, .units{0}},
    {.encoded{127}, .tens{1}, .units{5}},
    {.encoded{128}, .tens{42}, .units{42}},
    {.encoded{135}, .tens{42}, .units{42}},
    {.encoded{255}, .tens{42}, .units{42}},
  }};
  for(auto const &sample : samples) {
    auto target{cache};
    target.pixels.fill(99);
    darker::graphics::draw_grid_coordinate(cache, target, {.x{44}, .y{185}}, sample.encoded);
    for(unsigned int row{185}; row < 190; ++row) {
      for(unsigned int x{44}; x < 52; ++x) REQUIRE(target.pixels[row * 320 + x] == (x < 48 ? sample.tens : sample.units));
    }
    REQUIRE(target.pixels[185 * 320 + 43] == 99);
    REQUIRE(target.pixels[185 * 320 + 52] == 99);
    REQUIRE(target.pixels[190 * 320 + 44] == 99);
  }
}

TEST_CASE("Weapon icons enforce slot roles, preserve backgrounds and restore empty selections") {
  framework::render::indexed_cockpit_framebuffer cache;
  cache.pixels.fill(42);
  for(unsigned int selection{1}; selection <= 10; ++selection) {
    for(unsigned int row{8}; row < 20; ++row) std::fill_n(cache.pixels.begin() + row * 320 + 140 + selection * 8, 8, static_cast<std::uint8_t>(selection));
  }
  using darker::graphics::weapon_icon_slot;
  for(auto const slot : {weapon_icon_slot::primary, weapon_icon_slot::secondary}) {
    bool const primary{slot == weapon_icon_slot::primary};
    unsigned int const x{primary ? 260u : 268u};
    for(std::uint8_t selection{1}; selection <= 10; ++selection) {
      auto target{cache};
      target.pixels.fill(99);
      bool const primary_selection{selection == 1 || selection == 2 || selection == 3 || selection == 7};
      if(primary_selection != primary) {
        REQUIRE_THROWS(darker::graphics::draw_weapon_icon(cache, target, slot, selection));
        continue;
      }
      darker::graphics::draw_weapon_icon(cache, target, slot, selection);
      for(unsigned int row{195}; row < 207; ++row) {
        for(unsigned int column{x}; column < x + 8; ++column) REQUIRE(target.pixels[row * 320 + column] == selection);
      }
      REQUIRE(target.pixels[195 * 320 + x - 1] == 99);
      REQUIRE(target.pixels[195 * 320 + x + 8] == 99);
      darker::graphics::draw_weapon_icon(cache, target, slot, 0);
      REQUIRE(target.pixels[195 * 320 + x] == 42);
      REQUIRE(target.pixels[206 * 320 + x + 7] == 42);
    }
  }
}

TEST_CASE("Caero callbacks dispatch changed fields with row on the left and column on the right") {
  auto const cache{digit_cache()};
  auto target{cache};
  darker::graphics::caero_bitmap_state const state{.row{82}, .column{10}};
  darker::graphics::update_caero_bitmaps(cache, target, {}, state);
  REQUIRE(target.pixels[185 * 320 + 44] == 1);
  REQUIRE(target.pixels[185 * 320 + 48] == 0);
  REQUIRE(target.pixels[185 * 320 + 56] == 0);
  REQUIRE(target.pixels[185 * 320 + 60] == 2);
  target.pixels[185 * 320 + 44] = 77;
  darker::graphics::update_caero_bitmaps(cache, target, state, state);
  REQUIRE(target.pixels[185 * 320 + 44] == 77);
  darker::graphics::update_caero_bitmaps(cache, target, state, {});
  REQUIRE(target.pixels == cache.pixels);
}

TEST_CASE("Skimma bearing changes restore previous pixels and ordinary craft reject a third weapon") {
  framework::render::indexed_cockpit_framebuffer cache;
  for(std::size_t i{0}; i < cache.pixels.size(); ++i) cache.pixels[i] = static_cast<std::uint8_t>((i * 17 + 3) % 251);
  for(std::uint8_t first{1}; first <= 7; ++first) {
    for(std::uint8_t second{1}; second <= 7; ++second) {
      auto target{cache};
      auto fresh{cache};
      darker::graphics::skimma_bitmap_state const old_state{.bearing{first}};
      darker::graphics::skimma_bitmap_state const new_state{.bearing{second}};
      darker::graphics::update_skimma_bitmaps(cache, target, darker::graphics::craft::skimma, {}, old_state);
      darker::graphics::update_skimma_bitmaps(cache, target, darker::graphics::craft::skimma, old_state, new_state);
      darker::graphics::update_skimma_bitmaps(cache, fresh, darker::graphics::craft::skimma, {}, new_state);
      REQUIRE(target.pixels == fresh.pixels);
      darker::graphics::update_skimma_bitmaps(cache, target, darker::graphics::craft::skimma, new_state, {});
      REQUIRE(target.pixels == cache.pixels);
    }
  }
  auto target{cache};
  REQUIRE_THROWS(darker::graphics::update_skimma_bitmaps(cache, target, darker::graphics::craft::skimma, {}, {.weapons{0, 0, 1}}));
}

TEST_CASE("Large coordinate font uses blank glyphs for unavailable coordinates") {
  framework::render::indexed_cockpit_framebuffer cache;
  cache.pixels.fill(42);
  for(unsigned int digit{0}; digit < 12; ++digit) {
    for(unsigned int row{0}; row < 7; ++row) std::fill_n(cache.pixels.begin() + (8 + digit * 7 + row) * 320 + 312, 8, static_cast<std::uint8_t>(digit));
  }
  for(auto const encoded : {0, 1, 82, 127, 128, 255}) {
    auto target{cache};
    darker::graphics::draw_grid_coordinate(cache, target, {.x{56}, .y{41}}, static_cast<std::uint8_t>(encoded), darker::graphics::coordinate_font::large);
    auto const tens{target.pixels[41 * 320 + 56]};
    auto const units{target.pixels[47 * 320 + 71]};
    if(encoded == 0 || encoded >= 128) {
      REQUIRE(tens == 10);
      REQUIRE(units == 10);
    } else {
      REQUIRE(tens == (encoded == 1 ? 0 : 1));
      REQUIRE(units == (encoded == 1 ? 1 : encoded == 82 ? 0 : 5));
    }
    REQUIRE(target.pixels[48 * 320 + 56] == cache.pixels[48 * 320 + 56]);
  }
}
