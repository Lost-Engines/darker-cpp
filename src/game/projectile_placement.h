#pragma once

#include <array>
#include <cstdint>
#include "maths/view_basis.h"
#include "game/object_pose.h"

namespace darker::game {

struct launch_emitter {
  std::array<std::uint16_t, 3> position{};
  std::array<std::uint8_t, 3> fractions{};
  // Heading, pitch, roll, in native wrapping angle units.
  maths::attitude_angles angles{};
  std::uint16_t speed{0};
  std::uint8_t side_flags{0};
  std::uint8_t definition_strength{0};
};

object_pose place_projectile(launch_emitter const &emitter);

} // namespace darker::game
