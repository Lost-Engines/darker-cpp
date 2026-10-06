#include <catch2/catch_test_macros.hpp>
#include "graphics/screen_primitives.h"
#include "reference/screen_primitives_samples.h"

TEST_CASE("Model discs match native span construction and clipping", "[graphics][primitives]") {
  /// Retain subpixel-centred discs, the small-radius overlap asymmetry and viewport-edge clipping
  for(auto const &sample : darker::test_reference::disc_samples) {
    CAPTURE(sample.x, sample.y, sample.radius, sample.bottom);
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_disc(frame, {.x{sample.x}, .y{sample.y}}, sample.radius, 73, sample.bottom);
    std::uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const pixel : frame.pixels) fingerprint = (fingerprint ^ pixel) * 0x100000001b3;
    REQUIRE(fingerprint == sample.fingerprint);
  }
}

TEST_CASE("World lines retain original clipped endpoints", "[graphics][primitives]") {
  /// Check rejection flags and rounded endpoints before the shared HUD/world line rasteriser
  std::size_t index{0};
  for(auto const &sample : darker::test_reference::line_clip_samples) {
    CAPTURE(index);
    darker::graphics::pixel_position first{.x{sample.input[0]}, .y{sample.input[1]}};
    darker::graphics::pixel_position last{.x{sample.input[2]}, .y{sample.input[3]}};
    auto const visible{darker::graphics::clip_world_line(first, last, 168)};
    REQUIRE(visible == sample.visible);
    if(visible) {
      REQUIRE(first.x == sample.result[0]);
      REQUIRE(first.y == sample.result[1]);
      REQUIRE(last.x == sample.result[2]);
      REQUIRE(last.y == sample.result[3]);
    }
    ++index;
  }
}
