#pragma once

#include <array>
#include <cstdint>
#include "maths/world_coordinates.h"

namespace darker::maths {

struct direction_angles {
  std::uint16_t heading{0};
  std::uint16_t pitch{0};
};

std::uint16_t direction_index(std::uint16_t x, std::uint16_t y);

direction_angles direction_from_displacement(world_position const &displacement);
direction_angles object_target_direction(world_position const &position, world_position const &target);

} // namespace darker::maths
