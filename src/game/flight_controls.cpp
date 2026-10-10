#include "game/flight_controls.h"
#include <algorithm>
#include <bit>

namespace darker::game {
namespace {

int16_t word(int const value) noexcept {
  /// Preserve signed interpretation after each wrapping control operation
  return std::bit_cast<int16_t>(static_cast<uint16_t>(value));
}

uint16_t filtered_drive(steering_axis_state &state, uint16_t const previous, uint16_t const target, uint16_t const step) noexcept {
  /// 7C15 publishes the midpoint reference and its signed timestep-scaled drive
  state.reference = static_cast<uint16_t>(previous + (word(target - previous) >> 1));
  return static_cast<uint16_t>((word(state.reference) * word(step)) >> 8);
}

uint16_t keyboard_axis(steering_axis_state &state, bool const negative, bool const positive, bool const control, uint16_t const step) noexcept {
  /// 7C04/7C50 retain opposite-key behaviour, initial force and Ctrl's frame-step-dependent force changes
  auto const previous{state.keyboard_target};
  int const signed_previous{word(previous)};
  auto const magnitude{static_cast<uint16_t>(signed_previous < 0 ? -signed_previous : signed_previous)};
  uint16_t target{0};
  if(negative && positive) {
    target = previous;
    if(control) {
      auto const reduction{static_cast<uint16_t>(step << 2)};
      auto const reduced{magnitude < reduction ? 0 : magnitude - reduction};
      target = static_cast<uint16_t>(signed_previous < 0 ? -reduced : reduced);
    }
  } else if(negative || positive) {
    target = magnitude == 0 ? 0x0db6 : magnitude;
    if(control) target = static_cast<uint16_t>(std::clamp<int>(word(magnitude + static_cast<uint16_t>(step << 3)), -8191, 8191));
    if(negative) target = static_cast<uint16_t>(-target);
  }
  state.keyboard_target = target;
  return filtered_drive(state, previous, negative && positive && !control ? 0 : target, step);
}

uint16_t mouse_axis(steering_axis_state &state, uint16_t const position, uint16_t const sensitivity, uint16_t const step) noexcept {
  /// 7C73 combines a wrapping counter delta with the previous target, using asymmetric negative-side rounding
  auto const delta{static_cast<uint16_t>(position - state.previous_mouse)};
  state.previous_mouse = position;
  auto target{static_cast<uint16_t>(static_cast<uint32_t>(delta) * sensitivity)};
  auto const previous{state.mouse_target};
  auto const coefficient{static_cast<uint16_t>((128 - step) * 2)};
  int const old{word(previous)};
  auto const product{static_cast<uint32_t>(coefficient) * static_cast<uint16_t>(old < 0 ? -old : old)};
  if(old < 0) target = static_cast<uint16_t>(target - ((product + 128) >> 8));
  else target = static_cast<uint16_t>(target + (product >> 8));
  state.mouse_target = target;
  return filtered_drive(state, previous, target, step);
}

} // anonymous namespace

flight_steering update_flight_controls(flight_controls_state &state, flight_controls_input const input, uint16_t const frame_step) noexcept {
  /// 7AD6 gives nonzero keyboard drive priority over ordinary mouse processing; joystick/VR sources are separate paths
  flight_steering result{
    .bank{keyboard_axis(state.bank, input.left, input.right, input.control, frame_step)},
    .pitch{keyboard_axis(state.pitch, input.up, input.down, input.control, frame_step)},
  };
  if(result.bank != 0 || result.pitch != 0) return result;
  result.bank = mouse_axis(state.bank, input.mouse_x, input.mouse_sensitivity, frame_step);
  result.pitch = mouse_axis(state.pitch, static_cast<uint16_t>(-input.mouse_y), input.mouse_sensitivity, frame_step);
  return result;
}

} // namespace darker::game
