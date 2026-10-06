#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include "game/collision_sweep.h"
#include "reference/collision_sweep_samples.h"

TEST_CASE("Swept collision boxes match native intersection and impact rounding", "[game][collision]") {
  /// Include stationary segments, grazing endpoints, starting inside and approaches from both directions
  for(auto const &sample : darker::test_reference::collision_sweep_samples) {
    CAPTURE(sample.minimum, sample.maximum, sample.start, sample.end);
    auto const words{[](std::array<int, 3> const &values){
      return std::array<std::uint16_t, 3>{static_cast<std::uint16_t>(values[0]), static_cast<std::uint16_t>(values[1]), static_cast<std::uint16_t>(values[2])};
    }};
    darker::game::collision_box const box{.minimum{words(sample.minimum)}, .maximum{words(sample.maximum)}};
    auto end{words(sample.end)};
    REQUIRE(darker::game::sweep_collision_box(box, words(sample.start), end) == sample.hit);
    CHECK(end == words(sample.result));
  }
}
