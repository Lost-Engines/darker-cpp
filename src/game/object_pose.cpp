#include "game/object_pose.h"
#include <cassert>

namespace darker::game {

void displace_object(object_pose &pose, std::size_t const axis, std::int32_t const displacement) noexcept {
  /// Add a signed displacement to the wrapping coordinate word and fractional byte
  assert(axis < pose.position.size());
  auto const position{static_cast<std::uint32_t>(pose.position[axis]) * 256 + pose.fractions[axis] + static_cast<std::uint32_t>(displacement)};
  pose.position[axis] = static_cast<std::uint16_t>(position >> 8);
  pose.fractions[axis] = static_cast<std::uint8_t>(position);
}

} // namespace darker::game
