#include <catch2/catch_test_macros.hpp>
#include <array>
#include "game/player_crash.h"
#include "reference/player_crash_samples.h"

TEST_CASE("Player crash transition and rotation match native lifecycle updates", "[game][collision]") {
  /// Preserve already-expired objects, unrelated flags, wrapping deadlines and signed pitch convergence
  for(auto const &sample : darker::test_reference::player_crash_samples) {
    auto const &before{sample.before};
    darker::game::object_pose pose{
      .angles{static_cast<std::uint16_t>(before[0]), static_cast<std::uint16_t>(before[1]), static_cast<std::uint16_t>(before[2])},
      .speed{static_cast<std::uint16_t>(before[3])},
    };
    darker::game::player_crash_state state{.flags{static_cast<std::uint8_t>(before[4])}, .deadline{static_cast<std::uint16_t>(before[5])}};
    REQUIRE(darker::game::start_player_crash(pose, state, static_cast<std::uint16_t>(sample.input[0])) == sample.started);
    CHECK(std::array<int, 6>{pose.angles[0], pose.angles[1], pose.angles[2], pose.speed, state.flags, state.deadline} == sample.after);
    darker::game::advance_player_crash(pose, static_cast<std::uint16_t>(sample.input[1]));
    CHECK(std::array<int, 3>{pose.angles[0], pose.angles[1], pose.angles[2]} == sample.advanced);
  }
}
