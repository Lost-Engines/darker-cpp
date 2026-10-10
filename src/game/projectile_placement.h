#pragma once

#include <array>
#include <cstdint>
#include "game/object_pose.h"
#include "maths/view_basis.h"
#include "maths/world_coordinates.h"

namespace darker::game {

struct launch_emitter {
  maths::world_position position{};
  maths::position_fractions fractions{};
  // Heading, pitch, roll, in native wrapping angle units.
  maths::attitude_angles angles{};
  std::uint16_t speed{0};
  std::uint8_t side_flags{0};
  std::uint8_t definition_strength{0};
};

object_pose place_projectile(launch_emitter const &emitter);

} // namespace darker::game
