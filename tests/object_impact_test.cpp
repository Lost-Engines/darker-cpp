#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include "game/aircraft_combat.h"
#include "game/object_impact.h"
#include "reference/actor_impact_samples.h"
#include "reference/impact_samples.h"

TEST_CASE("Object impacts match native damage, angular kick and delayed destruction", "[game][impact]") {
  /// Compare complete native hit responses, including random-state consumption and existing expiry deadlines
  for(auto const &sample : darker::test_reference::impact_samples) {
    CAPTURE(sample.strength, sample.resistance, sample.mode, sample.damage, sample.flags, sample.seed);
    darker::game::object_impact_state state{
      .rotation{
        .pitch{static_cast<uint16_t>(sample.pitch)},
        .turn{static_cast<uint16_t>(sample.heading)}
      },
      .impact_accumulator{static_cast<uint16_t>(sample.accumulator)},
      .damage{static_cast<uint16_t>(sample.damage)},
      .update_entry{darker::game::object_update::surface_actor},
      .deadline{static_cast<uint16_t>(sample.deadline)},
      .flags{static_cast<uint8_t>(sample.flags)},
    };
    auto seed{static_cast<uint16_t>(sample.seed)};
    auto const effect{darker::game::apply_object_impact(state, static_cast<uint8_t>(sample.strength),
      static_cast<uint8_t>(sample.resistance), sample.mode == 2, static_cast<uint16_t>(sample.clock), seed)};
    std::array<int, 9> const actual{state.rotation.pitch, state.rotation.turn, state.impact_accumulator,
      state.damage, std::to_underlying(state.update_entry), state.deadline, state.flags, seed, static_cast<int>(effect)};
    CHECK(actual == sample.result);
  }
}

TEST_CASE("Ordinary object impact rejects separate removal paths before changing state", "[game][impact]") {
  /// Keep unsupported effect/removal branches explicit at the boundary
  darker::game::object_impact_state state{
    .update_entry{darker::game::object_update::surface_actor}
  };
  uint16_t seed{17};
  REQUIRE_THROWS_AS(darker::game::apply_object_impact(state, 1, 0, false, 0, seed), std::invalid_argument);
  CHECK(seed == 17);
  CHECK(state.damage == 0);
  state.update_entry = darker::game::object_update::inactive;
  REQUIRE_THROWS_AS(darker::game::apply_object_impact(state, 1, 1, false, 0, seed), std::invalid_argument);
  CHECK(seed == 17);
}

TEST_CASE("Special actor impacts match native removal and effect dispatch", "[game][impact]") {
  /// Zero resistance follows effect-only or delayed-removal paths without consuming random damage kicks
  for(auto const &sample : darker::test_reference::actor_impact_samples) {
    CAPTURE(sample);
    darker::game::object_definition definition{
      .role_data{darker::game::craft_definition_data{0, 0, 0, 0, 0, 0, 0, static_cast<uint8_t>(sample[1])}}
    };
    darker::game::scenario_actor actor;
    actor.parameters.definition = &definition;
    actor.parameters.update_entry = static_cast<darker::game::object_update>(sample[0]);
    actor.expiry = static_cast<uint16_t>(sample[4]);
    actor.flags = static_cast<uint8_t>(sample[3]);
    uint16_t random{17};
    auto const result{darker::game::hit_actor(actor, 52, static_cast<uint16_t>(sample[2]), random)};
    CHECK(result.effect == sample[5]);
    CHECK(result.at_actor == static_cast<bool>(sample[6]));
    CHECK(result.remove == static_cast<bool>(sample[7]));
    CHECK(actor.flags == sample[8]);
    CHECK(actor.expiry == sample[9]);
    CHECK(std::to_underlying(actor.parameters.update_entry) == sample[0]);
    CHECK(random == 17);
    CHECK(actor.awareness.level == 0);
    CHECK(actor.awareness.cooldown == 0);
  }
}
