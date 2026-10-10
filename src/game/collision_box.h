#pragma once

#include <array>
#include <cstdint>
#include "vectorstorm/aabb/aabb3.h"
#include "maths/world_coordinates.h"

namespace darker::game {

enum class collision_category : uint8_t {
  ground_target,
  protected_surface,
  fragile,
  diffuser,
};

struct collision_box {
  // column, row and height endpoints can wrap; retain their order and the native sweep rules
  aabb3<uint16_t> bounds{vec3<uint16_t>{0, 0, 0}, vec3<uint16_t>{0, 0, 0}};
  collision_category category{collision_category::ground_target};
};

} // namespace darker::game
