#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include <stdexcept>
#include "game/object_impact.h"
#include "reference/impact_samples.h"

TEST_CASE("Object impacts match native damage, angular kick and delayed destruction", "[game][impact]") {
  /// Compare complete native hit responses, including random-state consumption and existing expiry deadlines
  for(auto const &sample : darker::test_reference::impact_samples) {
    CAPTURE(sample.strength, sample.resistance, sample.mode, sample.damage, sample.flags, sample.seed);
    darker::game::object_impact_state state{
      .rotation{.pitch{static_cast<std::uint16_t>(sample.pitch)}, .heading{static_cast<std::uint16_t>(sample.heading)}},
      .impact_accumulator{static_cast<std::uint16_t>(sample.accumulator)},
      .damage{static_cast<std::uint16_t>(sample.damage)},
      .update_entry{0x8823},
      .deadline{static_cast<std::uint16_t>(sample.deadline)},
      .flags{static_cast<std::uint8_t>(sample.flags)},
    };
    auto seed{static_cast<std::uint16_t>(sample.seed)};
    auto const effect{darker::game::apply_object_impact(state, static_cast<std::uint8_t>(sample.strength),
      static_cast<std::uint8_t>(sample.resistance), sample.mode == 2, static_cast<std::uint16_t>(sample.clock), seed)};
    std::array<int, 9> const actual{state.rotation.pitch, state.rotation.heading, state.impact_accumulator,
      state.damage, state.update_entry, state.deadline, state.flags, seed, static_cast<int>(effect)};
    CHECK(actual == sample.result);
  }
}

TEST_CASE("Ordinary object impact rejects separate removal paths before changing state", "[game][impact]") {
  /// Keep unsupported effect/removal branches explicit at the boundary
  darker::game::object_impact_state state{.update_entry{0x8823}};
  std::uint16_t seed{17};
  REQUIRE_THROWS_AS(darker::game::apply_object_impact(state, 1, 0, false, 0, seed), std::invalid_argument);
  CHECK(seed == 17);
  CHECK(state.damage == 0);
  state.update_entry = 0;
  REQUIRE_THROWS_AS(darker::game::apply_object_impact(state, 1, 1, false, 0, seed), std::invalid_argument);
  CHECK(seed == 17);
}
