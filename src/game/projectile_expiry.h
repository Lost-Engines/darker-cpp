#pragma once

#include <cstdint>
#include "game/projectile_pool.h"
#include "game/skimma_weapons.h"

namespace darker::game {

struct projectile_references {
  uint16_t selected_target{0xffff};
  uint16_t reference_2449{0};
  uint16_t missile_view{0};
};

struct objective_counters {
  uint8_t completed{0};
  uint8_t outstanding{0};
};

projectile *expire_projectile(projectile_pool &pool, projectile &record, projectile_references &references,
  objective_counters &objectives, weapon_ring_state &ring);

} // namespace darker::game
