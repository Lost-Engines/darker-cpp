#include "game/beacon_light.h"
#include "reference/hud_integration_samples.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include "graphics/blit.h"
#include "graphics/cockpit.h"
#include "render/framebuffer.h"

TEST_CASE("Cockpit sheets follow native scenario configuration including the Delphi Skimma") {
  /// BC5F–BC77 captured sheet selections for configurations zero through four
  std::array<unsigned int,5> constexpr native_sheets{15,16,17,18,16};
  for(std::uint8_t configuration{0}; configuration < native_sheets.size(); ++configuration) {
    CHECK(darker::graphics::cockpit_resource_slot(configuration) == native_sheets[configuration]);
    CHECK(darker::graphics::cockpit_resource_slot(configuration | 0x30) == native_sheets[configuration]);
  }
}

TEST_CASE("Opaque blits clip source and destination together, including zero indices") {
  std::array<std::uint8_t, 640> source{};
  std::array<std::uint8_t, 640> target;
  target.fill(99);
  source[0] = 10;
  source[1] = 20;
  darker::graphics::copy_rectangle(source, target, {.x{-1}, .y{0}}, {.x{318}, .y{0}}, 4, 1);
  REQUIRE(target[318] == 99);
  REQUIRE(target[319] == 10);
  REQUIRE(target[320] == 99);
  darker::graphics::copy_rectangle(source, target, {.x{1}, .y{0}}, {.x{-1}, .y{1}}, 3, 1);
  REQUIRE(target[320] == 0);
  REQUIRE(target[321] == 0);
  REQUIRE(target[322] == 99);
  REQUIRE_THROWS(darker::graphics::copy_rectangle(source, target, {.x{0}, .y{0}}, {.x{0}, .y{0}}, -1, 1));
}

TEST_CASE("Mask row skips are independent and uncovered pixels are preserved") {
  std::array<std::uint8_t, 640> source;
  source.fill(7);
  std::array<std::uint8_t, 640> target;
  target.fill(99);
  std::array<darker::graphics::mask_row, 2> const mask{{{.skip{2}, .width{2}}, {.skip{0}, .width{1}}}};
  darker::graphics::copy_mask(source, target, {.x{0}, .y{0}}, {.x{10}, .y{0}}, mask);
  REQUIRE(target[11] == 99);
  REQUIRE(target[12] == 7);
  REQUIRE(target[13] == 7);
  REQUIRE(target[14] == 99);
  REQUIRE(target[330] == 7);
  REQUIRE(target[331] == 99);
}

TEST_CASE("Cockpit cache follows original overlapping source ranges") {
  framework::render::indexed_framebuffer sheet;
  for(unsigned int y{0}; y < 200; ++y) std::fill_n(sheet.pixels.begin() + y * 320, 320, static_cast<std::uint8_t>(y));
  auto const cache{darker::graphics::make_cockpit_cache(sheet)};
  REQUIRE(cache.pixels[135 * 320] == 135);
  REQUIRE(cache.pixels[136 * 320] == 96);
  REQUIRE(cache.pixels.back() == 199);
  auto const view{framework::render::fit_viewport(960, 720, 320, 240)};
  REQUIRE(view.width == 960);
  REQUIRE(view.height == 720);
}

TEST_CASE("HUD transitions restore cached pixels and respect Skimma output limits") {
  framework::render::indexed_cockpit_framebuffer cache;
  for(std::size_t i{0}; i < cache.pixels.size(); ++i) cache.pixels[i] = static_cast<std::uint8_t>((i / 320 + i % 320) % 256);
  for(auto const type : {darker::graphics::craft::caero, darker::graphics::craft::skimma, darker::graphics::craft::upgraded_skimma}) {
    auto const components{darker::graphics::cockpit_components(type)};
    for(std::size_t i{0}; i < components.size(); ++i) {
      auto screen{cache};
      auto const maximum{static_cast<std::uint8_t>(darker::graphics::instrument_limit(type, i))};
      darker::graphics::update_instrument(cache, screen, type, i, 0, maximum);
      REQUIRE(screen.pixels != cache.pixels);
      darker::graphics::update_instrument(cache, screen, type, i, maximum, 0);
      REQUIRE(screen.pixels == cache.pixels);
    }
  }
  REQUIRE(darker::graphics::instrument_limit(darker::graphics::craft::skimma, 2) == 16);
  REQUIRE(darker::graphics::instrument_limit(darker::graphics::craft::upgraded_skimma, 2) == 20);
  auto screen{cache};
  REQUIRE_THROWS(darker::graphics::update_instrument(cache, screen, darker::graphics::craft::skimma, 2, 0, 17));
}

TEST_CASE("Engine alternate state redraws a strip even when count is unchanged") {
  framework::render::indexed_cockpit_framebuffer cache;
  for(std::size_t i{0}; i < cache.pixels.size(); ++i) cache.pixels[i] = static_cast<std::uint8_t>((i / 320 + i % 320) % 256);
  auto screen{cache};
  auto const type{darker::graphics::craft::caero};
  darker::graphics::update_instrument(cache, screen, type, 7, 0, 1);
  auto const normal{screen};
  darker::graphics::update_instrument(cache, screen, type, 7, 1, 129);
  REQUIRE(screen.pixels != normal.pixels);
  darker::graphics::update_instrument(cache, screen, type, 7, 129, 1);
  REQUIRE(screen.pixels == normal.pixels);
  darker::graphics::update_instrument(cache, screen, type, 7, 1, 0);
  REQUIRE(screen.pixels == cache.pixels);
}

TEST_CASE("Caero frame-edge overlays match native blit selection", "[graphics][cockpit]") {
  /// Coordinate-varying source pixels detect incorrect masks and viewport sampling offsets
  framework::render::indexed_cockpit_framebuffer cache{}, target{};
  for(size_t i{0}; i < cache.pixels.size(); ++i) cache.pixels[i] = static_cast<uint8_t>(i * 37 + 11);
  target.pixels.fill(99);
  darker::graphics::draw_caero_frame_edges(cache,target);
  uint64_t fingerprint{0xcbf29ce484222325};
  for(auto pixel : target.pixels) fingerprint = (fingerprint ^ pixel) * 0x100000001b3;
  CHECK(fingerprint == darker::test_reference::frame_edge_fingerprint);
}

TEST_CASE("Radar grid readout follows the native beacon lookup and paired invalidation", "[graphics][cockpit]") {
  /// Sweep both map seams and every nearest-beacon boundary using actual position words
  for(auto const &sample : darker::test_reference::grid_samples) {
    CHECK(darker::game::beacon_grid_coordinates({sample.column,sample.row}) == std::array<uint8_t,2>{sample.x,sample.y});
  }
}

TEST_CASE("Skimma cockpit edges and moving shield pulse match native blits", "[graphics][cockpit]") {
  /// Compare entire indexed surfaces, including pixels that must remain unlit during startup
  framework::render::indexed_cockpit_framebuffer cache{}, target{};
  for(size_t i{0}; i < cache.pixels.size(); ++i) cache.pixels[i] = static_cast<uint8_t>(i * 37 + 11);
  auto const fingerprint{[](auto const &pixels) {
    uint64_t result{0xcbf29ce484222325};
    for(auto const pixel : pixels) result = (result ^ pixel) * 0x100000001b3;
    return result;
  }};
  target.pixels.fill(99);
  darker::graphics::draw_skimma_frame_edges(cache, target);
  CHECK(fingerprint(target.pixels) == darker::test_reference::skimma_frame_edge_fingerprint);
  for(uint8_t phase{0}; phase < 24; ++phase) {
    CAPTURE(phase);
    target = cache;
    darker::graphics::draw_skimma_shield_startup(cache, target, phase);
    CHECK(fingerprint(target.pixels) == darker::test_reference::shield_pulse_fingerprints[phase]);
  }
}
