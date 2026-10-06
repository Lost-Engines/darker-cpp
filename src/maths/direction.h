#pragma once

#include <array>
#include <cstdint>

namespace darker::maths {

struct direction_angles {
  std::uint16_t heading{0};
  std::uint16_t pitch{0};
};

std::uint16_t direction_index(std::uint16_t x, std::uint16_t y);

direction_angles direction_from_displacement(std::array<std::uint16_t, 3> const &displacement);
direction_angles object_target_direction(std::array<std::uint16_t, 3> const &position, std::array<std::uint16_t, 3> const &target);

} // namespace darker::maths
