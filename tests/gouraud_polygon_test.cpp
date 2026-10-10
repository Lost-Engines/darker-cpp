#include <catch2/catch_test_macros.hpp>
#include "graphics/gouraud_polygon.h"
#include "reference/gouraud_samples.h"

TEST_CASE("Gouraud polygons match original palette bands and clipping", "[graphics][gouraud]") {
  /// Compare all pixels for winding, narrow spans, clipped corners and positive/negative colour gradients
  size_t index{0};
  for(auto const &sample : darker::test_reference::gouraud_samples) {
    CAPTURE(index);
    std::array<darker::graphics::shaded_vertex, 4> vertices{};
    for(size_t i{0}; i < sample.count; ++i) {
      auto const &source{sample.vertices[i]};
      vertices[i] = {
        .position{static_cast<int16_t>(source[0]), static_cast<int16_t>(source[1])},
        .shade{static_cast<uint16_t>((sample.base + source[2]) * 256 + source[2] + 128)}
      };
    }
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_gouraud_polygon(frame, std::span{vertices}.first(sample.count), {
      .right{319},
      .bottom{168},
    });
    uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const pixel : frame.pixels) fingerprint = (fingerprint ^ pixel) * 0x100000001b3;
    REQUIRE(fingerprint == sample.fingerprint);
    ++index;
  }
}
