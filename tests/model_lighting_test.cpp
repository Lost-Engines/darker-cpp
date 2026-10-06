#include <catch2/catch_test_macros.hpp>
#include "graphics/model_lighting.h"
#include "reference/model_lighting_samples.h"

TEST_CASE("Distance shading matches native ramps and beacon strength selection", "[graphics][lighting]") {
  /// Include city/underground ramps, byte boundaries, clamping and signed near-path depths
  for(auto const &sample : darker::test_reference::model_lighting_samples) {
    CAPTURE(sample.count, sample.depth, sample.light, sample.near);
    darker::graphics::distance_shading const lighting{sample.count};
    auto const colours{lighting.colours(static_cast<std::uint16_t>(sample.depth),
      sample.near ? darker::graphics::model_path::near_clipped : darker::graphics::model_path::direct,
      static_cast<std::uint8_t>(sample.light))};
    std::uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const shade : colours.shades) fingerprint = (fingerprint ^ shade) * 0x100000001b3;
    fingerprint = (fingerprint ^ colours.dynamic) * 0x100000001b3;
    REQUIRE(fingerprint == sample.fingerprint);
  }
}

TEST_CASE("Every generated distance shade matches the original table", "[graphics][lighting]") {
  /// Check all 28 palette shades in every row of both original table sizes
  for(auto const &sample : darker::test_reference::shade_table_samples) {
    darker::graphics::distance_shading const lighting{sample.count};
    std::uint64_t fingerprint{0xcbf29ce484222325};
    for(unsigned int row{0}; row < sample.count; ++row) {
      auto const colours{lighting.colours(static_cast<std::uint16_t>(row * 256), darker::graphics::model_path::direct, 255)};
      for(auto const shade : colours.shades) fingerprint = (fingerprint ^ shade) * 0x100000001b3;
    }
    REQUIRE(fingerprint == sample.fingerprint);
  }
}
