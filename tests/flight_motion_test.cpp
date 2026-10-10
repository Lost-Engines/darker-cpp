#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include "game/flight_motion.h"
#include "reference/flight_motion_samples.h"

TEST_CASE("Shared flight integration matches native movement and speed trajectories", "[game][flight]") {
  /// Carry C++ state between steps and compare the original fractional coordinates, velocities and integer speed
  darker::game::object_pose pose{};
  std::uint16_t horizontal{0};
  std::uint16_t vertical{0};
  for(auto const &sample : darker::test_reference::flight_motion_samples) {
    CAPTURE(sample.frame, sample.target_h, sample.target_v, sample.time_h, sample.time_v, sample.heading, sample.pitch);
    if(sample.frame == 0) {
      for(std::size_t axis{0}; axis < 3; ++axis) {
        pose.position[axis] = static_cast<std::uint16_t>(sample.before[axis]);
        pose.fractions[axis] = static_cast<std::uint8_t>(sample.before[axis + 3]);
      }
      horizontal = static_cast<std::uint16_t>(sample.before[6]);
      vertical = static_cast<std::uint16_t>(sample.before[7]);
      pose.speed = static_cast<std::uint16_t>(sample.before[8]);
    }
    darker::game::advance_horizontal_flight(pose, horizontal, static_cast<std::uint16_t>(sample.target_h),
      static_cast<std::uint16_t>(sample.time_h), static_cast<std::uint16_t>(sample.heading), static_cast<std::uint16_t>(sample.pitch));
    darker::game::advance_vertical_flight(pose, vertical, static_cast<std::uint16_t>(sample.target_v), static_cast<std::uint16_t>(sample.time_v));
    darker::game::measure_flight_speed(pose, horizontal, vertical);
    std::array<int, 9> const actual{pose.position.column, pose.position.row, pose.position.height, pose.fractions.column, pose.fractions.row, pose.fractions.height,
      horizontal, vertical, pose.speed};
    REQUIRE(actual == sample.after);
  }
}
