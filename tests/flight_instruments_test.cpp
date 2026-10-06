#include <catch2/catch_test_macros.hpp>
#include "graphics/flight_instruments.h"
#include "reference/flight_instruments_samples.h"

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
