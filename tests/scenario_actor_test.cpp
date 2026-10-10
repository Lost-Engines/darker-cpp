#include <catch2/catch_test_macros.hpp>
#include <utility>
#include <array>
#include <stdexcept>
#include "game/object_definitions.h"
#include "game/scenario_actor.h"
#include "reference/scenario_actor_samples.h"

TEST_CASE("Scenario actors match original surface, special and static constructors", "[game][actors]") {
  /// Compare constructor outputs with BE07, including counted attributes and target/program interpretation
  for(auto const &sample : darker::test_reference::scenario_actor_samples) {
    auto const &v{sample.input};
    darker::resources::scenario_placement placement{
      .form{static_cast<darker::resources::placement_form>(v[0])},
      .definition_slot{static_cast<std::uint8_t>(v[2])},
      .counted{v[3] != 0},
      .attributes{static_cast<std::uint8_t>(v[5])},
      .heading{static_cast<std::uint16_t>(v[4])},
      .position{static_cast<std::uint16_t>(v[6]), static_cast<std::uint16_t>(v[7])},
    };
    for(std::size_t i{0}; i < placement.motion.size(); ++i) placement.motion[i] = static_cast<std::uint8_t>(v[8 + i]);
    if(v[0] != 2) {
      placement.script_or_target = static_cast<std::uint16_t>(v[14]);
      if(v[14] < 0x8000) placement.program_offset = static_cast<std::size_t>(v[17] + v[14]);
    }
    auto const actor{darker::game::make_scenario_actor(placement, darker::game::original_object_definitions[v[2]],
      0x400, static_cast<std::int16_t>(v[15]), static_cast<std::uint8_t>(v[16]), static_cast<std::uint8_t>(v[1]), 0)};
    auto const &p{actor.parameters};
    CAPTURE(v);
    CHECK(std::array<int, 31>{actor.pose.position[0], actor.pose.position[1], actor.pose.position[2],
      actor.pose.angles.heading, actor.pose.speed, actor.flags, actor.attributes, actor.fade,
      std::to_underlying(p.update_entry), actor.target_token, actor.current_cell, p.flags_4c, p.angular_response,
      p.motion.bank_response, p.motion.bank_limit, p.motion.turn_response, actor.previous_position[0], actor.previous_position[1], actor.previous_position[2],
      actor.behaviour.attack_control, actor.behaviour.awareness_threshold, actor.behaviour.awareness_decay, actor.behaviour.awareness_rise, actor.behaviour.awareness_strength, actor.behaviour.evasion,
      actor.index, actor.definition_slot, static_cast<int>(actor.route ? actor.route->cursor : actor.script.continuation),
      static_cast<int>(actor.route ? actor.route->cursor : actor.script.checkpoint),
      actor.script.stopped, actor.route ? actor.route->origin : actor.script.deadline} == sample.output);
    CHECK(actor.pose.fractions == std::array<std::uint8_t, 3>{});
    CHECK(p.definition == &darker::game::original_object_definitions[v[2]]);
    CHECK(p.model_token == 0x400);
  }
}

TEST_CASE("Underground moving actors require route setup", "[game][actors]") {
  /// Reject an incomplete setup path rather than spawning route vehicles as ordinary aircraft
  darker::resources::scenario_placement placement{};
  CHECK_THROWS_AS(darker::game::make_scenario_actor(placement, darker::game::original_object_definitions[19], 0, 0, 1, 2, 0), std::invalid_argument);
}
