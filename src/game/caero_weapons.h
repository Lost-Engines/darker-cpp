#pragma once

#include <cstdint>
#include "game/caero_energy.h"
#include "game/projectile_pool.h"

namespace darker::game {

struct caero_fire_result {
  projectile *shot{nullptr};
  bool ready{false};
};

caero_fire_result fire_caero_weapon(projectile_pool &pool, caero_energy_state &energy, launch_emitter const &emitter,
  uint8_t selection, uint8_t player_flags, bool trigger_pressed, uint16_t model, uint16_t clock, uint16_t target = 0xffff, bool underground = false);

} // namespace darker::game
