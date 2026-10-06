#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <span>
#include "graphics/flat_polygon.h"
#include "reference/flat_polygon_samples.h"

TEST_CASE("Flat polygon coverage matches native clipping and VGA scanline boundaries", "[graphics][polygon]") {
  /// Compare the complete indexed frame against coverage captured before the original VGA writes
  std::size_t index{0};
  for(auto const &sample : darker::test_reference::flat_polygon_samples) {
    CAPTURE(index, sample.count, sample.pixels, sample.right, sample.bottom);
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_flat_polygon(frame, std::span{sample.vertices}.first(sample.count), 37, sample.right, sample.bottom);
    std::uint64_t fingerprint{0xcbf29ce484222325};
    int pixels{0};
    for(auto const pixel : frame.pixels) {
      fingerprint = (fingerprint ^ pixel) * 0x100000001b3;
      if(pixel != 0) ++pixels;
    }
    CHECK(pixels == sample.pixels);
    REQUIRE(fingerprint == sample.fingerprint);
    ++index;
  }
}
