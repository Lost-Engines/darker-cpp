#include <catch2/catch_test_macros.hpp>
#include "game/radar_coverage.h"
#include "graphics/radar_beacons.h"
#include "reference/radar_beacon_samples.h"

TEST_CASE("Radio coverage and energy tower pixels match native radar frames", "[radar]") {
  /// Compare original coverage masks, contact decisions and all tower pixels with independent native calls
  for(auto const &sample : darker::test_reference::radar_beacon_samples) {
    CAPTURE(sample[0],sample[1],sample[2],sample[3],sample[4]);
    darker::game::city_map cells{};
    for(size_t y{0}; y < 128; y += 9) {
      for(size_t x{0}; x < 128; x += 9) {
        constexpr std::array<uint8_t,5> states{128,0,160,255,64};
        cells[y*128 + x] = {.type{1},.state{states[(x/9 + y/9*15 + sample[4]) % 5]}};
      }
    }
    for(size_t y{0}; y < 4; ++y) {
      for(size_t x{0}; x < 4; ++x) cells[(13 + y*36)*128 + 13 + x*36] = {.type{12},.state{static_cast<uint8_t>(sample[3] & (1u << (y*4+x)) ? 32 : 0)}};
    }
    std::array<uint16_t,2> const position{static_cast<uint16_t>(sample[0]),static_cast<uint16_t>(sample[1])};
    auto const coverage{darker::game::make_radar_coverage(cells,position,sample[5] != 0)};
    CHECK(coverage.mask == sample[6]);
    for(size_t i{0}; i < 8; ++i) CHECK(coverage.contains(static_cast<uint8_t>(sample[7+i*2]),static_cast<uint8_t>(sample[8+i*2])) == (sample[23+i] != 0));
    framework::render::indexed_cockpit_framebuffer actual{}, expected{};
    darker::graphics::draw_radar_beacons(actual,cells,{position[0],position[1]},static_cast<uint16_t>(sample[2]),coverage);
    for(size_t i{0}; i < sample[31]; ++i) expected.pixels[sample[33+i*3]*320 + sample[32+i*3]] = static_cast<uint8_t>(sample[34+i*3]);
    CHECK(actual.pixels == expected.pixels);
  }
}
