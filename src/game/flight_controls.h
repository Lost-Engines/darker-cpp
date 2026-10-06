#pragma once

#include <cstdint>

namespace darker::game {

struct steering_axis_state {
  std::uint16_t keyboard_target{0};
  std::uint16_t mouse_target{0};
  std::uint16_t previous_mouse{0};
  std::uint16_t reference{0};
};

struct flight_controls_state {
  steering_axis_state bank{};
  steering_axis_state pitch{};
};

struct flight_controls_input {
  bool left{false};
  bool right{false};
  bool up{false};
  bool down{false};
  bool control{false};
  bool look_around{false};
  std::uint16_t mouse_x{0};                                                    // original accumulated mouse-motion counters
  std::uint16_t mouse_y{0};
  std::uint16_t mouse_sensitivity{12};
};

struct flight_steering {
  std::uint16_t bank{0};
  std::uint16_t pitch{0};
};

flight_steering update_flight_controls(flight_controls_state &state, flight_controls_input input, std::uint16_t frame_step) noexcept;

} // namespace darker::game
