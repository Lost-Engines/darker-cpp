#pragma once

#include <cstdint>
#include <utility>

namespace darker::game {

// Caero cockpit selections are one-based; they are not object-definition indices
// the two paired weapons have separate selections for their firing stages
enum class caero_weapon : uint8_t {
  none,
  pinner_direct,
  pinner_mimic,
  dual_launch_capsule,
  diffuser_gas,
  diffuser_trigger,
  brent_ground,
  dual_launch_follow_up,
  caero_weapon,
  chargeable,
  brent_hunter,
};

// Skimma slots are zero-based; zero is a real slot, not 'no selection'
enum class skimma_weapon : uint8_t { primary, ground, dual_launch };

constexpr uint8_t caero_definition_slot(caero_weapon weapon) noexcept {
  /// Translate an already validated selection into the native projectile catalogue
  return static_cast<uint8_t>(std::to_underlying(weapon) - 1);
}

constexpr bool uses_primary_trigger(caero_weapon weapon) noexcept {
  switch(weapon) {
    case caero_weapon::pinner_direct:
    case caero_weapon::pinner_mimic:
    case caero_weapon::dual_launch_capsule:
    case caero_weapon::dual_launch_follow_up:
      return true;
    default:
      return false;
  }
}

constexpr bool uses_secondary_trigger(caero_weapon weapon) noexcept {
  switch(weapon) {
    case caero_weapon::diffuser_gas:
    case caero_weapon::diffuser_trigger:
    case caero_weapon::brent_ground:
    case caero_weapon::caero_weapon:
    case caero_weapon::chargeable:
    case caero_weapon::brent_hunter:
      return true;
    default:
      return false;
  }
}

} // namespace darker::game
