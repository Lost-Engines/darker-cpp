#include "reference/hud_integration_samples.h"
#include <catch2/catch_test_macros.hpp>
#include "graphics/flight_instruments.h"
#include "reference/flight_instruments_samples.h"
#include "reference/skimma_instruments_samples.h"

TEST_CASE("Live flight instrument producers match native damage, charging, altitude and speed fields", "[graphics][cockpit]") {
  /// Include damage flashing and the original word/byte boundaries rather than inferring gauges from labels
  for(auto const &sample : darker::test_reference::flight_instruments_samples) {
    auto const &input{sample.input};
    darker::game::caero_flight_state state{
      .pose{.position{0, 0, static_cast<std::uint16_t>(input[2])}}, .damage{.damage{static_cast<std::uint16_t>(input[0])}},
      .energy{.boost{static_cast<std::uint16_t>(input[1])}},
    };
    auto const values{darker::graphics::measure_caero_instruments(state, static_cast<std::uint16_t>(input[3]))};
    CHECK(std::array<int, 6>{values.altitude, values.impact, values.damage_lights, values.power_cells, values.charging,
      darker::graphics::skimma_speed_instrument(static_cast<std::uint16_t>(input[4]), input[5] != 0)} == sample.output);
  }
}

TEST_CASE("Skimma warning and shield gauges match native startup deadlines", "[graphics][cockpit]") {
  /// Exercise deadline wrapping, depleted reserves and all shield startup phases
  for(auto const &sample : darker::test_reference::skimma_instruments_samples) {
    auto const &v{sample.input};
    auto deadline{static_cast<std::uint16_t>(v[5])};
    auto const result{darker::graphics::measure_skimma_instruments(static_cast<std::uint16_t>(v[0]),
      static_cast<std::uint16_t>(v[1]), v[2] != 0, v[3] != 0, static_cast<std::uint16_t>(v[4]), deadline)};
    CAPTURE(v);
    auto const range{darker::graphics::skimma_shield_strips(result.shield_startup)};
    CHECK(std::array<int, 6>{result.low_altitude, result.shield, result.shield_startup, deadline, range.first, range.end} == sample.output);
  }
}

TEST_CASE("Caero engine lamp retains native stall hysteresis", "[graphics][cockpit]") {
  /// Include both directions through the 200–409 retained-state interval
  for(auto const &sample : darker::test_reference::engine_samples) {
    CHECK(darker::graphics::caero_engine_indicator(sample.previous,sample.enabled,sample.speed) == sample.result);
  }
}
