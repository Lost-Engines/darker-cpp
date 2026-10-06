#pragma once

#include <cstdint>
#include "game/object_pose.h"

namespace darker::game {

struct actor_attitude {
  std::uint16_t pitch_rate{0};
  std::uint16_t bank_rate{0};
};

struct actor_steering_parameters {
  std::uint16_t response{0};
  std::uint16_t bank_response{0};
  std::uint16_t bank_limit{0};
  std::uint16_t turn_response{0};
};

std::uint16_t steer_actor(object_pose &pose, actor_attitude &state, actor_steering_parameters parameters,
  std::uint16_t desired_pitch, std::uint16_t turn_drive, std::uint16_t frame_step) noexcept;
void advance_actor_speed(object_pose &pose, std::uint8_t desired_speed, std::uint8_t acceleration,
  std::uint8_t deceleration, std::uint16_t frame_step) noexcept;

} // namespace darker::game
