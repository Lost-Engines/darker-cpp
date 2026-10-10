#pragma once

#include <cstdint>
#include "game/object_pose.h"
#include "game/time.h"

namespace darker::game {

struct actor_attitude {
  uint16_t pitch_rate{0};
  uint16_t bank_rate{0};
};

struct actor_steering_parameters {
  uint16_t response{0};
  uint16_t bank_response{0};
  uint16_t bank_limit{0};
  uint16_t turn_response{0};
};

uint16_t steer_actor(object_pose &pose, actor_attitude &state, actor_steering_parameters parameters,
  uint16_t desired_pitch, uint16_t turn_drive, game_duration frame_step) noexcept;
void advance_actor_speed(object_pose &pose, uint8_t desired_speed, uint8_t acceleration,
  uint8_t deceleration, game_duration frame_step) noexcept;

} // namespace darker::game
