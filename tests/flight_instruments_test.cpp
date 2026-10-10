#include <catch2/catch_test_macros.hpp>
#include "graphics/flight_instruments.h"
#include "reference/flight_instruments_samples.h"
#include "reference/hud_integration_samples.h"
#include "reference/receiver_indicator_samples.h"
#include "reference/skimma_bearing_samples.h"
#include "reference/skimma_instruments_samples.h"

TEST_CASE("Live flight instrument producers match native damage, charging, altitude and speed fields", "[graphics][cockpit]") {
  /// Include damage flashing and the original word/byte boundaries rather than inferring gauges from labels
  for(auto const &sample : darker::test_reference::flight_instruments_samples) {
    auto const &input{sample.input};
    darker::game::caero_flight_state state{
      .pose{
        .position{
          .column{0},
          .row{0},
          .height{static_cast<uint16_t>(input[2])}
        }
      },
      .damage{
        .damage{static_cast<uint16_t>(input[0])}
      },
      .energy{
        .boost{static_cast<uint16_t>(input[1])}
      },
    };
    auto const values{darker::graphics::measure_caero_instruments(state, static_cast<uint16_t>(input[3]))};
    CHECK(std::array<int, 6>{values.altitude, values.impact, values.damage_lights, values.power_cells, values.charging,
      darker::graphics::skimma_speed_instrument(static_cast<uint16_t>(input[4]), input[5] != 0)} == sample.output);
  }
}

TEST_CASE("Skimma warning and shield gauges match native startup deadlines", "[graphics][cockpit]") {
  /// Exercise deadline wrapping, depleted reserves and all shield startup phases
  for(auto const &sample : darker::test_reference::skimma_instruments_samples) {
    auto const &v{sample.input};
    auto deadline{static_cast<uint16_t>(v[5])};
    auto const result{darker::graphics::measure_skimma_instruments(static_cast<uint16_t>(v[0]),
      static_cast<uint16_t>(v[1]), v[2] != 0, v[3] != 0, static_cast<uint16_t>(v[4]), deadline)};
    CAPTURE(v);
    auto const range{darker::graphics::skimma_shield_strips(result.shield_startup)};
    CHECK(std::array<int, 6>{result.low_altitude, result.shield, result.shield_startup, deadline, range.first, range.end} == sample.output);
  }
}

TEST_CASE("Caero engine lamp retains native stall hysteresis", "[graphics][cockpit]") {
  /// Include both directions through the 200–409 retained-state interval
  for(auto const &sample : darker::test_reference::engine_samples) {
    CHECK(darker::graphics::caero_engine_indicator(sample.previous, sample.enabled, sample.speed) == sample.result);
  }
}

TEST_CASE("Skimma mission-bearing symbols match the native heading sectors", "[graphics][cockpit]") {
  /// Verify wrapped positions, relative heading and the negative sentinel against 576E
  for(auto const &v : darker::test_reference::skimma_bearing_samples) {
    CAPTURE(v);
    CHECK(darker::graphics::skimma_mission_bearing(static_cast<uint16_t>(v[0]), static_cast<uint16_t>(v[1]),
      static_cast<uint16_t>(v[2]), static_cast<uint16_t>(v[3])) == v[4]);
  }
}

TEST_CASE("Nayas receiver activity matches the original message and return conditions", "[graphics][cockpit]") {
  /// Include active text, stopped scripts, incomplete objectives and the docked-player flag across the timer cycle
  for(auto const &v : darker::test_reference::receiver_indicator_samples) {
    CAPTURE(v);
    CHECK(darker::graphics::caero_receiver_indicator(static_cast<uint16_t>(v[0]), v[1] != 0, v[2] != 0,
      v[3] != 0, static_cast<uint8_t>(v[4])) == v[5]);
  }
}
