#pragma once

#include <cstdint>

namespace darker::game {

// Values retain the original callback addresses for comparison with native traces.
enum class object_update : std::uint16_t {
  inactive = 0x0000,
  effect_only = 0x6ed3,
  supply_pad = 0x7e49,
  caero = 0x7e7f,
  skimma = 0x8108,
  tunnel_actor = 0x8609,
  surface_actor = 0x8823,
  falling_aircraft = 0x8daa,
  departing_aircraft = 0x8ddd,
  ground_vehicle = 0x8f3b,
  retired_actor = 0xc002,
  mimic = 0xcbce,
  dual_launch = 0xcbe7,
  homing_projectile = 0xcc61,
  direct_projectile = 0xcc64,
  chargeable = 0xcc68,
  tunnel_player = 0xd510,
};

} // namespace darker::game
