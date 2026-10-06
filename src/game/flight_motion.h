#pragma once

#include <cstdint>
#include "game/object_pose.h"

namespace darker::game {

void advance_speed_motion(object_pose &pose, std::uint16_t speed, std::uint16_t frame_step) noexcept;

void advance_horizontal_flight(object_pose &pose, std::uint16_t &velocity, std::uint16_t target,
  std::uint16_t timestep, std::uint16_t heading, std::uint16_t pitch) noexcept;
void advance_vertical_flight(object_pose &pose, std::uint16_t &velocity, std::uint16_t target, std::uint16_t timestep) noexcept;
void measure_flight_speed(object_pose &pose, std::uint16_t horizontal, std::uint16_t vertical) noexcept;

} // namespace darker::game
