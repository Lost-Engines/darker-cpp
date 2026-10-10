#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include "graphics/sky_ground.h"
#include "reference/sky_ground_samples.h"

TEST_CASE("Sky and ground bands match native horizon projection", "[graphics][background]") {
  /// Compare both viewport heights, pitch directions and bank quadrants against captured native spans
  for(auto const &sample : darker::test_reference::sky_ground_samples) {
    CAPTURE(sample.height, sample.pitch, sample.roll);
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_sky_ground(frame, {
      .pitch{sample.pitch},
      .roll{sample.roll}
    }, {160, static_cast<std::int16_t>(sample.height / 2)}, sample.height);
    std::uint64_t hash{14695981039346656037ull};
    for(int i{0}; i < sample.height * 320; ++i) {
      hash ^= frame.pixels[i];
      hash *= 1099511628211ull;
    }
    CHECK(hash == sample.hash);
  }
}
