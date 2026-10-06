#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include "graphics/procedural_hud.h"
#include "reference/procedural_samples.h"

namespace {

std::uint64_t checksum(framework::render::indexed_cockpit_framebuffer const &frame) {
  /// Compare every indexed pixel, including unchanged background
  std::uint64_t result{14695981039346656037ULL};
  for(auto const pixel : frame.pixels) result = (result ^ pixel) * 1099511628211ULL;
  return result;
}

} // namespace

TEST_CASE("Attitude endpoint calculation and line rasterisation match 289 original-code frames") {
  for(auto const &sample : darker::test_reference::attitude) {
    INFO("pitch=" << sample.pitch << " roll=" << sample.roll);
    framework::render::indexed_cockpit_framebuffer frame{};
    auto const line{darker::graphics::calculate_attitude(sample.pitch, sample.roll, 0, false)};
    darker::graphics::draw_screen_line(frame, line.first, line.last, line.colour);
    REQUIRE(checksum(frame) == sample.checksum);
  }
}

TEST_CASE("Target bytecode reproduces native geometry and per-step colour changes") {
  using darker::graphics::target_marker;
  std::array<target_marker, 3> const markers{target_marker::small, target_marker::large, target_marker::skimma_aim};
  for(std::size_t i{0}; i < markers.size(); ++i) {
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_target_marker(frame, markers[i], {.x{160}, .y{84}}, i == 2 ? 14 : 243, i == 2 ? 14 : 233);
    REQUIRE(checksum(frame) == darker::test_reference::markers[i]);
  }
}

TEST_CASE("Original line endpoints and tie rules hold for both slopes, reversals, axes and wide spans") {
  for(auto const &sample : darker::test_reference::lines) {
    INFO("from " << sample.first.x << ',' << sample.first.y << " to " << sample.last.x << ',' << sample.last.y);
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_screen_line(frame, sample.first, sample.last, 14);
    REQUIRE(checksum(frame) == sample.checksum);
  }
}

TEST_CASE("Attitude endpoints and shading preserve native signs and full table quadrants") {
  for(auto const &sample : darker::test_reference::endpoints) {
    auto const line{darker::graphics::calculate_attitude(sample.pitch, sample.roll, sample.pitch_high, sample.alternate)};
    REQUIRE(line.first.x == sample.line.first.x);
    REQUIRE(line.first.y == sample.line.first.y);
    REQUIRE(line.last.x == sample.line.last.x);
    REQUIRE(line.last.y == sample.line.last.y);
    REQUIRE(line.colour == sample.line.colour);
  }
}

TEST_CASE("Caero fixed outlines reproduce the original paired-path bytecode") {
  framework::render::indexed_cockpit_framebuffer frame{};
  darker::graphics::draw_attitude_surround(frame, 0);
  REQUIRE(checksum(frame) == darker::test_reference::attitude_surround_checksum);
}
