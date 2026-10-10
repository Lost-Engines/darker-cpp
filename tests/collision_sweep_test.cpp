#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include "game/collision_sweep.h"
#include "maths/world_coordinates.h"
#include "reference/collision_sweep_samples.h"

TEST_CASE("Swept collision boxes match native intersection and impact rounding", "[game][collision]") {
  /// Include stationary segments, grazing endpoints, starting inside and approaches from both directions
  for(auto const &sample : darker::test_reference::collision_sweep_samples) {
    CAPTURE(sample.minimum, sample.maximum, sample.start, sample.end);
    auto const words{[](std::array<int, 3> const &values){
      return darker::maths::world_position{
        .column{static_cast<std::uint16_t>(values[0])},
        .row{static_cast<std::uint16_t>(values[1])},
        .height{static_cast<std::uint16_t>(values[2])}
      };
    }};
    auto const point{[](std::array<int, 3> const &values){
      return vec3<std::uint16_t>{vec3<int>{values[0], values[1], values[2]}};
    }};
    darker::game::collision_box const box{
      .bounds{point(sample.minimum), point(sample.maximum)}
    };
    auto end{words(sample.end)};
    REQUIRE(darker::game::sweep_collision_box(box, words(sample.start), end) == sample.hit);
    CHECK(end == words(sample.result));
  }
}
