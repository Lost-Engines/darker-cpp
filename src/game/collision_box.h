#pragma once

#include <array>
#include <cstdint>
#include "vectorstorm/aabb/aabb3.h"
#include "maths/world_coordinates.h"

namespace darker::game {

struct collision_box {
  // column, row and height endpoints can wrap; retain their order and the native sweep rules
  aabb3<uint16_t> bounds{vec3<uint16_t>{0, 0, 0}, vec3<uint16_t>{0, 0, 0}};
  uint8_t category{0};
};

} // namespace darker::game
