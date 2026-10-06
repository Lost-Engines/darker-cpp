#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace darker::game {

struct object_pose {
  std::array<std::uint16_t, 3> position{};
  std::array<std::uint8_t, 3> fractions{};
  // Heading, pitch and roll in native wrapping angle units.
  std::array<std::uint16_t, 3> angles{};
  std::uint16_t speed{0};
};

void displace_object(object_pose &pose, std::size_t axis, std::int32_t displacement) noexcept;

} // namespace darker::game
