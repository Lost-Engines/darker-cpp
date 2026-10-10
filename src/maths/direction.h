#pragma once

#include <array>
#include <cstdint>
#include "maths/world_coordinates.h"

namespace darker::maths {

struct direction_angles {
  uint16_t heading{0};
  uint16_t pitch{0};
};

uint16_t direction_index(uint16_t x, uint16_t y);

direction_angles direction_from_displacement(world_position const &displacement);
direction_angles object_target_direction(world_position const &position, world_position const &target);

} // namespace darker::maths
