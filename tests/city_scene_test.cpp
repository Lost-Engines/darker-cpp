#include <catch2/catch_test_macros.hpp>
#include "graphics/city_scene.h"
#include "reference/city_scan_samples.h"
#include "reference/object_window_samples.h"

TEST_CASE("City scans preserve native candidate order and map boundaries", "[graphics][city]") {
  /// Match circular spans, heading halves, full-pitch scans and empty-cell skips
  darker::game::city_map cells{};
  for(std::size_t i{0}; i < cells.size(); ++i) cells[i].type = i % 7 == 0 ? 0 : 1;
  std::vector<std::uint16_t> candidates;
  std::size_t index{0};
  for(auto const &sample : darker::test_reference::city_scan_samples) {
    CAPTURE(index, sample.column, sample.row, sample.heading, sample.pitch, sample.radius);
    darker::graphics::collect_city_cells(cells, static_cast<std::uint8_t>(sample.column), static_cast<std::uint8_t>(sample.row),
      {.heading{static_cast<std::uint16_t>(sample.heading)}, .pitch{static_cast<std::uint16_t>(sample.pitch)}}, sample.radius, candidates);
    REQUIRE(candidates.size() == sample.count);
    std::uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const cell : candidates) {
      for(auto const byte : {cell & 255, cell >> 8}) fingerprint = (fingerprint ^ byte) * 0x100000001b3;
    }
    REQUIRE(fingerprint == sample.fingerprint);
    ++index;
  }
}

TEST_CASE("Moving-object visibility matches the native wrapping byte window", "[graphics][city]") {
  /// Exercise both axes and both native radii before narrower projection arithmetic can alias distant coordinates
  for(auto const &sample : darker::test_reference::object_window_samples) {
    darker::graphics::city_view const view{.column{static_cast<uint16_t>(sample[0])}, .row{static_cast<uint16_t>(sample[1])}, .radius{sample[2]}};
    std::array<uint16_t,3> const position{static_cast<uint16_t>(sample[3]), static_cast<uint16_t>(sample[4]), 0};
    CAPTURE(sample);
    REQUIRE(darker::graphics::within_object_window(view, position) == static_cast<bool>(sample[5]));
  }
}
