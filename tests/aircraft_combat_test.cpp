#include <catch2/catch_test_macros.hpp>
#include <array>
#include "game/aircraft_combat.h"
#include "game/object_definitions.h"
#include "reference/aircraft_combat_samples.h"

TEST_CASE("Aircraft hit volumes match native extent sweeps", "[combat]") {
  /// Check both hit admission and native impact rounding at full altitude scale
  for(auto const &v : darker::test_reference::aircraft_sweeps) {
    CAPTURE(v);
    darker::game::object_pose const target{.position{static_cast<uint16_t>(v[0]), static_cast<uint16_t>(v[1]), static_cast<uint16_t>(v[2])}};
    std::array<uint16_t, 3> const start{static_cast<uint16_t>(v[5]), static_cast<uint16_t>(v[6]), static_cast<uint16_t>(v[7])};
    std::array<uint16_t, 3> end{static_cast<uint16_t>(v[8]), static_cast<uint16_t>(v[9]), static_cast<uint16_t>(v[10])};
    CHECK(darker::game::sweep_aircraft(target, v[3], v[4], start, end) == (v[11] != 0));
    CHECK(end == std::array<uint16_t, 3>{static_cast<uint16_t>(v[12]), static_cast<uint16_t>(v[13]), static_cast<uint16_t>(v[14])});
  }
}

TEST_CASE("Falling aircraft match the original destruction callback", "[combat]") {
  /// Check coupled bank decay, dive and displacement, retaining fractional positions
  for(auto const &v : darker::test_reference::aircraft_falls) {
    CAPTURE(v);
    darker::game::scenario_actor actor;
    actor.pose.position = {static_cast<uint16_t>(v[0]), static_cast<uint16_t>(v[1]), static_cast<uint16_t>(v[2])};
    actor.attitude = {static_cast<uint16_t>(v[3]), static_cast<uint16_t>(v[4])};
    actor.pose.angles = {static_cast<uint16_t>(v[5]), static_cast<uint16_t>(v[6]), static_cast<uint16_t>(v[7])};
    actor.pose.speed = static_cast<uint16_t>(v[8]);
    darker::game::advance_falling_aircraft(actor, v[9]);
    std::array<int, 12> const actual{actor.pose.position[0], actor.pose.position[1], actor.pose.position[2], actor.attitude.pitch_rate, actor.attitude.bank_rate,
      actor.pose.angles[0], actor.pose.angles[1], actor.pose.angles[2], actor.pose.speed, actor.pose.fractions[0], actor.pose.fractions[1], actor.pose.fractions[2]};
    for(size_t i{0}; i < actual.size(); ++i) CHECK(actual[i] == v[i + 10]);
  }
}

TEST_CASE("First mission gun checks match original aim, timing and hit decisions", "[combat]") {
  /// Native firing executes through the ray intersection, intercepting only damage application and effect spawning
  for(auto const &v : darker::test_reference::aircraft_guns) {
    CAPTURE(v);
    darker::game::scenario_actor actor;
    actor.definition_slot = 19;
    actor.behaviour[0] = static_cast<uint8_t>(v[16]);
    actor.selected_target = 0xd986;
    actor.parameters.definition = &darker::game::original_object_definitions[19];
    actor.pose.position = {10000, 10000, 3000};
    actor.pose.angles = {static_cast<uint16_t>(v[0]), static_cast<uint16_t>(v[1]), 0};
    darker::game::object_pose const player{.position{static_cast<uint16_t>(v[9]), static_cast<uint16_t>(v[10]), static_cast<uint16_t>(v[11])}};
    uint16_t random{static_cast<uint16_t>(v[8])};
    auto const shot{darker::game::fire_skimma_gun(actor, player, v[7], v[12],
      {.heading{static_cast<uint16_t>(v[2])}, .pitch{static_cast<uint16_t>(v[3])}}, v[4], v[5], v[6], random)};
    CHECK(shot.has_value() == (v[13] != 0));
    CHECK((shot && shot->hit) == (v[14] != 0));
    CHECK(random == v[15]);
  }
}
