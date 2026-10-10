#include "game/dual_launch.h"
#include <bit>

namespace darker::game {

uint16_t dual_launch_separation(object_pose const &detonator, object_pose const &capsule) noexcept {
  /// CBE7 combines wrapped 841C horizontal distance with absolute vertical separation
  auto const x{static_cast<uint16_t>(detonator.position.column-capsule.position.column)};
  auto const y{static_cast<uint16_t>(detonator.position.row-capsule.position.row)};
  auto const z{static_cast<uint16_t>(capsule.position.height-detonator.position.height)};
  auto const horizontal{static_cast<uint16_t>((x ^ (x & 0x8000 ? 0xffff : 0)) + (y & 0x8000 ? -y : y))};
  auto const vertical{static_cast<uint16_t>(capsule.position.height < detonator.position.height ? -z : z)};
  return static_cast<uint16_t>((horizontal >> 1)+(vertical >> 3));
}

std::optional<uint8_t> dual_launch_impact(object_pose const &origin, object_pose const &victim, bool const airborne) noexcept {
  /// 6DB5 bounds each category pass; CD13 derives strength from wrapped squared distances and a word division
  unsigned int const radius{airborne ? 0x500u : 0x1e0u};
  for(size_t axis{0}; axis < 2; ++axis) {
    if(static_cast<uint16_t>(victim.position[axis]-origin.position[axis]+radius) >= radius*2) return std::nullopt;
  }
  auto const z{static_cast<uint16_t>(victim.position.height-origin.position.height)};
  auto const height{static_cast<uint16_t>(z ^ (victim.position.height < origin.position.height ? 0xffff : 0)) >> 8};
  if(height >= 0x28) return std::nullopt;
  unsigned int distance{static_cast<unsigned int>(height*height)};
  for(size_t axis{0}; axis < 2; ++axis) {
    auto const delta{static_cast<uint16_t>(origin.position[axis]-victim.position[axis])};
    auto const scaled{std::bit_cast<int8_t>(static_cast<uint8_t>(delta >> 5))};
    distance += static_cast<unsigned int>(scaled*scaled);
  }
  auto const squared{static_cast<uint16_t>(distance*4)};
  if((squared >> 8) >= (airborne ? 25 : 6)) return std::nullopt;
  auto const denominator{static_cast<uint16_t>(squared+0x400)};
  auto quotient{static_cast<uint16_t>((0x1000000u+denominator)/denominator)};
  quotient = static_cast<uint16_t>(quotient+0xc00);
  return static_cast<uint8_t>(static_cast<uint16_t>(quotient*2) >> 8);
}

} // namespace darker::game
