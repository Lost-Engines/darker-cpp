#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include "game/object_pose.h"

namespace darker::game {

struct vehicle_route {
  size_t cursor{0};
  uint16_t origin{0xf000};
  uint8_t command{0};
  bool removed{false};
};

void advance_vehicle_route(vehicle_route &route, object_pose &pose, uint8_t &flags,
  std::span<std::byte const> program, uint16_t clock, int16_t model_height);

} // namespace darker::game
