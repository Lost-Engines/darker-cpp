#include <catch2/catch_test_macros.hpp>
#include "game/caero_flight.h"
#include "reference/caero_flight_samples.h"

namespace {

darker::game::caero_flight_state read_state(std::array<int, 26> const &values) {
  /// Map native object/global fields into the named callback state
  auto const word{[&](std::size_t const i){ return static_cast<std::uint16_t>(values[i]); }};
  return {
    .pose{.position{word(0), word(1), word(2)},
      .fractions{static_cast<std::uint8_t>(values[3]), static_cast<std::uint8_t>(values[4]), static_cast<std::uint8_t>(values[5])},
      .angles{word(6), word(7), word(8)}, .speed{word(9)}},
    .damage{.rotation{.pitch{word(10)}, .turn{word(11)}}, .damage{word(21)}},
    .energy{.buffer{word(17)}, .reserve{word(18)}, .boost{word(19)},
      .incoming_display{static_cast<std::uint8_t>(values[24])}, .reserve_display{static_cast<std::uint8_t>(values[25])}},
    .horizontal_velocity{word(12)}, .vertical_velocity{word(13)}, .pitch_assist_rate{word(14)},
    .active_boost{word(15)}, .forward_bias{word(16)}, .repair_phase{word(20)}, .startup_energy{word(22)}, .flying{values[23] != 0},
  };
}

std::array<int, 26> write_state(darker::game::caero_flight_state const &state) {
  /// Compare every persistent field observed by the native callback trace
  return {state.pose.position.column, state.pose.position.row, state.pose.position.height,
    state.pose.fractions.column, state.pose.fractions.row, state.pose.fractions.height,
    state.pose.angles.heading, state.pose.angles.pitch, state.pose.angles.roll, state.pose.speed,
    state.damage.rotation.pitch, state.damage.rotation.turn, state.horizontal_velocity, state.vertical_velocity,
    state.pitch_assist_rate, state.active_boost, state.forward_bias, state.energy.buffer, state.energy.reserve,
    state.energy.boost, state.repair_phase, state.damage.damage, state.startup_energy, state.flying,
    state.energy.incoming_display, state.energy.reserve_display};
}

} // namespace

TEST_CASE("Complete Caero flight updates match native persistent-state traces", "[game][flight]") {
  /// Exercise startup transition, manual/assisted pitch, braking, boost, charging and coupled motion in one callback
  std::size_t index{0};
  for(auto const &sample : darker::test_reference::caero_flight_samples) {
    CAPTURE(index);
    auto state{read_state(sample.before)};
    auto const &input{sample.input};
    darker::game::city_map cells{};
    cells.fill({.type{1}, .state{static_cast<std::uint8_t>(input[12])}});
    darker::game::advance_caero_flight(state,
      {.angular_response{static_cast<std::uint16_t>(input[7])}, .drive_multiplier{static_cast<std::uint16_t>(input[8])},
        .vertical_bias{static_cast<std::int8_t>(input[11])}, .desired_height{static_cast<std::uint16_t>(input[9])},
        .height_reference{static_cast<std::uint16_t>(input[10])}},
      {.bank_drive{static_cast<std::uint16_t>(input[1])}, .pitch_drive{static_cast<std::uint16_t>(input[2])},
        .engine_flags{static_cast<std::uint8_t>(input[3])}, .altitude_hold{input[4] != 0}, .brake{input[5] != 0}, .boost_cheat{input[6] != 0}},
      static_cast<std::uint16_t>(input[0]), cells);
    auto const actual{write_state(state)};
    for(std::size_t field{0}; field < actual.size(); ++field) {
      CAPTURE(field, sample.before[field]);
      REQUIRE(actual[field] == sample.after[field]);
    }
    ++index;
  }
}

TEST_CASE("Caero hangar charging pauses with the engine off and resumes without losing charge", "[game][flight][hangar]") {
  /// Retail startup keeps accumulated energy and pips while the engine is disabled
  darker::game::city_map const cells{};
  darker::game::caero_flight_state running{}, switched{};
  for(int tick{0}; tick < 64; ++tick) {
    darker::game::advance_caero_flight(running, {}, {.engine_flags{1}}, 16, cells);
  }
  switched = running;
  REQUIRE(switched.energy.boost != 0);
  for(int tick{0}; tick < 64; ++tick) {
    darker::game::advance_caero_flight(switched, {}, {.engine_flags{0}}, 16, cells);
    CHECK(write_state(switched) == write_state(running));
  }
  for(int tick{0}; tick < 192; ++tick) {
    darker::game::advance_caero_flight(running, {}, {.engine_flags{1}}, 16, cells);
    darker::game::advance_caero_flight(switched, {}, {.engine_flags{1}}, 16, cells);
  }
  CHECK(write_state(switched) == write_state(running));
  CHECK(switched.energy.boost == 0xa000);
  CHECK_FALSE(switched.flying);
  CHECK(darker::game::activate_caero_boost(switched));
}

TEST_CASE("Caero boost activation matches the original reserve threshold and byte writes", "[game][flight]") {
  /// Include insufficient reserve, an exact pip and retriggering an already active boost
  for(auto const &sample : darker::test_reference::caero_boost_samples) {
    darker::game::caero_flight_state state{
      .energy{.boost{static_cast<std::uint16_t>(sample.reserve)}}, .active_boost{static_cast<std::uint16_t>(sample.active)},
    };
    REQUIRE(darker::game::activate_caero_boost(state) == (sample.accepted != 0));
    REQUIRE(state.energy.boost == sample.result_reserve);
    REQUIRE(state.active_boost == sample.result_active);
  }
}
