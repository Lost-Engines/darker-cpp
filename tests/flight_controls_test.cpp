#include <catch2/catch_test_macros.hpp>
#include "game/flight_controls.h"
#include "reference/flight_controls_samples.h"

TEST_CASE("Keyboard and mouse steering preserve native smoothing and source priority", "[game][controls]") {
  /// Compare persistent targets, mouse history, published references and both timestep-scaled steering drives
  std::size_t index{0};
  for(auto const &sample : darker::test_reference::flight_controls_samples) {
    CAPTURE(index);
    auto const axis{[&](std::size_t const i){
      return darker::game::steering_axis_state{
        .keyboard_target{static_cast<std::uint16_t>(sample.before[i])}, .mouse_target{static_cast<std::uint16_t>(sample.before[i + 1])},
        .previous_mouse{static_cast<std::uint16_t>(sample.before[i + 2])}, .reference{static_cast<std::uint16_t>(sample.before[i + 3])},
      };
    }};
    darker::game::flight_controls_state state{.bank{axis(0)}, .pitch{axis(4)}};
    auto const held{sample.input[0]};
    auto const drive{darker::game::update_flight_controls(state,
      {.left{(held & 1) != 0}, .right{(held & 2) != 0}, .up{(held & 4) != 0}, .down{(held & 8) != 0}, .control{(held & 16) != 0},
        .mouse_x{static_cast<std::uint16_t>(sample.input[1])}, .mouse_y{static_cast<std::uint16_t>(sample.input[2])},
        .mouse_sensitivity{static_cast<std::uint16_t>(sample.input[4])}}, static_cast<std::uint16_t>(sample.input[3]))};
    std::array<int, 8> const actual{state.bank.keyboard_target, state.bank.mouse_target, state.bank.previous_mouse, state.bank.reference,
      state.pitch.keyboard_target, state.pitch.mouse_target, state.pitch.previous_mouse, state.pitch.reference};
    REQUIRE(actual == sample.after);
    REQUIRE(drive.bank == sample.drive[0]);
    REQUIRE(drive.pitch == sample.drive[1]);
    ++index;
  }
}
