#include <array>
#include <utility>
#include <catch2/catch_test_macros.hpp>
#include "game/mission_combat.h"
#include "reference/objective_counter_samples.h"

TEST_CASE("Script objective adjustments match native wrapping and signed clamping", "[game][missions]") {
  /// Clearing the obligation to defeat an aircraft does not remove the aircraft itself
  for(auto const &s : darker::test_reference::objective_counter_samples) {
    CAPTURE(s);
    std::vector<darker::game::scenario_actor> actors(static_cast<size_t>(s[0]));
    for(auto &actor : actors) actor.attributes = 1;
    darker::game::mission_combat combat{std::move(actors)};
    std::array const program{std::byte{0x33},static_cast<std::byte>(s[1]),std::byte{0x23}};
    darker::game::mission_script script;
    darker::game::mission_context context{
      .program{program}
    };
    context.adjust_objectives = [&](uint8_t const operand){ combat.adjust_objectives(operand); return combat.remaining_objectives() == 0; };
    CHECK(darker::game::advance_mission_script(script,context) == 2);
    CHECK(combat.remaining_objectives() == static_cast<unsigned>(s[2]));
    CHECK(context.objectives_complete == (s[2] == 0));
    CHECK(combat.actors.size() == static_cast<size_t>(s[0]));
    CHECK(combat.completed_objectives == 0);
  }
}
