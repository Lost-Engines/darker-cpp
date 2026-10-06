#pragma once

#include <array>
#include <cstdint>
#include "game/collision_box.h"

namespace darker::game {

bool sweep_collision_box(collision_box const &box, std::array<std::uint16_t, 3> const &start,
  std::array<std::uint16_t, 3> &end) noexcept;

} // namespace darker::game
