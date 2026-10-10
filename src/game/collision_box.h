#pragma once

#include <array>
#include <cstdint>
#include "maths/world_coordinates.h"
#include "vectorstorm/aabb/aabb3.h"

namespace darker::game {

struct collision_box {
  // Column, row and height endpoints can wrap; retain their order and the native sweep rules.
  aabb3<std::uint16_t> bounds{vec3<std::uint16_t>{0, 0, 0}, vec3<std::uint16_t>{0, 0, 0}};
  std::uint8_t category{0};
};

} // namespace darker::game
