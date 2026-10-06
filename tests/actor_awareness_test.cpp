#include <catch2/catch_test_macros.hpp>
#include <array>
#include "game/actor_awareness.h"
#include "reference/awareness_samples.h"

TEST_CASE("Actor engagement and cooldown accounting match native arithmetic", "[game][actors]") {
  /// Cover proximity falloff, wrapping positions, byte-rate changes and native shift-count semantics
  for(auto const &sample : darker::test_reference::awareness_samples) {
    auto const &input{sample.input};
    darker::game::object_pose const actor{.position{static_cast<std::uint16_t>(input[0]), static_cast<std::uint16_t>(input[1]), 0}};
    darker::game::object_pose const player{.position{static_cast<std::uint16_t>(input[2]), static_cast<std::uint16_t>(input[3]), 0}};
    darker::game::actor_awareness state{.level{static_cast<std::uint16_t>(input[4])}, .cooldown{static_cast<std::uint16_t>(input[5])}};
    darker::game::advance_actor_awareness(state, actor, player,
      {.decay{static_cast<std::uint8_t>(input[6])}, .rise{static_cast<std::uint8_t>(input[7])},
        .strength{static_cast<std::uint8_t>(input[8])}, .cooldown_shift{static_cast<std::uint8_t>(input[9])}},
      static_cast<std::uint16_t>(input[10]));
    CHECK(std::array<int, 2>{state.level, state.cooldown} == sample.output);
  }
}
