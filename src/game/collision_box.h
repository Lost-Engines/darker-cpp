#pragma once

#include <array>
#include <cstdint>

namespace darker::game {

struct collision_box {
  std::array<std::uint16_t, 3> minimum{};                                    // column, row and height in the collision routine's units
  std::array<std::uint16_t, 3> maximum{};
  std::uint8_t category{0};
};

} // namespace darker::game
