#include "game/object_pose.h"
#include <bit>
#include <cassert>

namespace darker::game {

void displace_object(object_pose &pose, std::size_t const axis, std::int32_t const displacement) noexcept {
  /// Add a signed displacement to the wrapping coordinate word and fractional byte
  assert(axis < pose.position.size());
  auto const position{static_cast<std::uint32_t>(pose.position[axis]) * 256 + pose.fractions[axis] + static_cast<std::uint32_t>(displacement)};
  pose.position[axis] = static_cast<std::uint16_t>(position >> 8);
  pose.fractions[axis] = static_cast<std::uint8_t>(position);
}

std::uint16_t horizontal_distance(std::array<std::uint16_t, 3> const &position, std::array<std::uint16_t, 3> const &target) noexcept {
  /// 841C sums wrapped coordinate differences, using one's complement for negative X and negation for negative Y
  auto const x{std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(position[0] - target[0]))};
  auto const y{std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(position[1] - target[1]))};
  return static_cast<std::uint16_t>((x < 0 ? ~x : x) + (y < 0 ? -y : y));
}

} // namespace darker::game
