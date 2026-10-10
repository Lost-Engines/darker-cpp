#pragma once

#include <cstdint>
#include "game/object_pose.h"

namespace darker::game {

void advance_speed_motion(object_pose &pose, uint16_t speed, uint16_t frame_step) noexcept;

void advance_horizontal_flight(object_pose &pose, uint16_t &velocity, uint16_t target,
  uint16_t timestep, uint16_t heading, uint16_t pitch) noexcept;
void advance_vertical_flight(object_pose &pose, uint16_t &velocity, uint16_t target, uint16_t timestep) noexcept;
void measure_flight_speed(object_pose &pose, uint16_t horizontal, uint16_t vertical) noexcept;

} // namespace darker::game
