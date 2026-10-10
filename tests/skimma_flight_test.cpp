#include <catch2/catch_test_macros.hpp>
#include "game/skimma_flight.h"
#include "reference/skimma_flight_samples.h"

TEST_CASE("Complete Skimma flight updates match native persistent-state traces", "[game][flight]") {
  /// Both craft parameter sets cover steering saturation, altitude assistance, speed settings, braking and shield recharge
  size_t index{0};
  for(auto const &sample : darker::test_reference::skimma_flight_samples) {
    CAPTURE(index);
    auto const &before{sample.before};
    auto const word{[&](size_t const i){
      return static_cast<uint16_t>(before[i]);
    }};
    darker::game::skimma_flight_state state{
      .pose{
        .position{
          .column{word(0)},
          .row{word(1)},
          .height{word(2)}
        },
        .fractions{
          .column{static_cast<uint8_t>(before[3])},
          .row{static_cast<uint8_t>(before[4])},
          .height{static_cast<uint8_t>(before[5])}
        },
        .angles{
          .heading{word(6)},
          .pitch{word(7)},
          .roll{word(8)}
        },
        .speed{word(9)}
      },
      .damage{
        .rotation{
          .pitch{word(10)},
          .turn{word(11)}
        },
        .shield_charge{word(15)}
      },
      .horizontal_velocity{word(12)},
      .vertical_velocity{word(13)},
      .pitch_assist_rate{word(14)},
    };
    auto const &input{sample.input};
    darker::game::advance_skimma_flight(state,
      {
        .angular_response{static_cast<uint16_t>(input[3])},
        .vertical_bias{static_cast<int8_t>(input[6])}
      },
      {
        .bank_drive{static_cast<uint16_t>(input[1])},
        .pitch_drive{static_cast<uint16_t>(input[2])},
        .forward_setting{static_cast<uint16_t>(input[4])},
        .brake{input[5] != 0}
      },
      static_cast<uint16_t>(input[0]));
    std::array<int, 16> const actual{state.pose.position.column, state.pose.position.row, state.pose.position.height,
      state.pose.fractions.column, state.pose.fractions.row, state.pose.fractions.height,
      state.pose.angles.heading, state.pose.angles.pitch, state.pose.angles.roll, state.pose.speed,
      state.damage.rotation.pitch, state.damage.rotation.turn, state.horizontal_velocity, state.vertical_velocity,
      state.pitch_assist_rate, state.damage.shield_charge};
    for(size_t field{0}; field < actual.size(); ++field) {
      CAPTURE(field, before[field]);
      REQUIRE(actual[field] == sample.after[field]);
    }
    ++index;
  }
}
