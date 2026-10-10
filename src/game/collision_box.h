#pragma once

#include <array>
#include <cstdint>
#include "maths/world_coordinates.h"

namespace darker::game {

struct collision_box {
  maths::world_position minimum{};                                    // column, row and height in the collision routine's units
  maths::world_position maximum{};
  std::uint8_t category{0};
};

} // namespace darker::game
