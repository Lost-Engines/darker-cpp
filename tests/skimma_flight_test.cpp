#include <catch2/catch_test_macros.hpp>
#include "game/skimma_flight.h"
#include "reference/skimma_flight_samples.h"

TEST_CASE("Complete Skimma flight updates match native persistent-state traces", "[game][flight]") {
  /// Both craft parameter sets cover steering saturation, altitude assistance, speed settings, braking and shield recharge
  std::size_t index{0};
  for(auto const &sample : darker::test_reference::skimma_flight_samples) {
    CAPTURE(index);
    auto const &before{sample.before};
    auto const word{[&](std::size_t const i){ return static_cast<std::uint16_t>(before[i]); }};
    darker::game::skimma_flight_state state{
      .pose{.position{word(0), word(1), word(2)},
        .fractions{static_cast<std::uint8_t>(before[3]), static_cast<std::uint8_t>(before[4]), static_cast<std::uint8_t>(before[5])},
        .angles{word(6), word(7), word(8)}, .speed{word(9)}},
      .damage{.rotation{.pitch{word(10)}, .turn{word(11)}}, .shield_charge{word(15)}},
      .horizontal_velocity{word(12)}, .vertical_velocity{word(13)}, .pitch_assist_rate{word(14)},
    };
    auto const &input{sample.input};
    darker::game::advance_skimma_flight(state,
      {.angular_response{static_cast<std::uint16_t>(input[3])}, .vertical_bias{static_cast<std::int8_t>(input[6])}},
      {.bank_drive{static_cast<std::uint16_t>(input[1])}, .pitch_drive{static_cast<std::uint16_t>(input[2])},
        .forward_setting{static_cast<std::uint16_t>(input[4])}, .brake{input[5] != 0}},
      static_cast<std::uint16_t>(input[0]));
    std::array<int, 16> const actual{state.pose.position[0], state.pose.position[1], state.pose.position[2],
      state.pose.fractions[0], state.pose.fractions[1], state.pose.fractions[2],
      state.pose.angles[0], state.pose.angles[1], state.pose.angles[2], state.pose.speed,
      state.damage.rotation.pitch, state.damage.rotation.turn, state.horizontal_velocity, state.vertical_velocity,
      state.pitch_assist_rate, state.damage.shield_charge};
    for(std::size_t field{0}; field < actual.size(); ++field) {
      CAPTURE(field, before[field]);
      REQUIRE(actual[field] == sample.after[field]);
    }
    ++index;
  }
}
