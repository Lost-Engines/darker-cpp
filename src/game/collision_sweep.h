#pragma once

#include <array>
#include <cstdint>
#include "game/collision_box.h"
#include "maths/world_coordinates.h"

namespace darker::game {

bool sweep_collision_box(collision_box const &box, maths::world_position const &start,
  maths::world_position &end) noexcept;

} // namespace darker::game
