#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "maths/view_basis.h"
#include "maths/world_coordinates.h"

namespace darker::game {

struct object_pose {
  maths::world_position position{};
  maths::position_fractions fractions{};
  // Heading, pitch and roll in native wrapping angle units.
  maths::attitude_angles angles{};
  std::uint16_t speed{0};
};

void displace_object(object_pose &pose, std::size_t axis, std::int32_t displacement) noexcept;
std::uint16_t horizontal_distance(maths::world_position const &position, maths::world_position const &target) noexcept;

} // namespace darker::game
