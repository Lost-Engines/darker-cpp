#pragma once

#include <cstdint>
#include "game/time.h"

namespace darker::game {

struct steering_axis_state {
  uint16_t keyboard_target{0};
  uint16_t mouse_target{0};
  uint16_t previous_mouse{0};
  uint16_t reference{0};

  uint16_t keyboard(bool negative, bool positive, bool control, game_duration step) noexcept;
  uint16_t mouse(uint16_t position, uint16_t sensitivity, game_duration step) noexcept;

private:
  uint16_t filtered_drive(uint16_t previous, uint16_t target, game_duration step) noexcept;
};

struct flight_controls_input;
struct flight_steering;

struct flight_controls_state {
  steering_axis_state bank{};
  steering_axis_state pitch{};

  flight_steering update(flight_controls_input input, game_duration frame_step) noexcept;
};

struct flight_controls_input {
  bool left{false};
  bool right{false};
  bool up{false};
  bool down{false};
  bool control{false};
  bool look_around{false};
  uint16_t mouse_x{0};                                                         // original accumulated mouse-motion counters
  uint16_t mouse_y{0};
  uint16_t mouse_sensitivity{12};
};

struct flight_steering {
  uint16_t bank{0};
  uint16_t pitch{0};
};

} // namespace darker::game
