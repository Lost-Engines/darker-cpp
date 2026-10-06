#pragma once

#include <cstdint>
#include "game/projectile_pool.h"
#include "game/skimma_weapons.h"

namespace darker::game {

struct projectile_references {
  std::uint16_t selected_target{0xffff};
  std::uint16_t reference_2449{0};
  std::uint16_t missile_view{0};
};

struct objective_counters {
  std::uint8_t completed{0};
  std::uint8_t outstanding{0};
};

projectile *expire_projectile(projectile_pool &pool, projectile &record, projectile_references &references,
  objective_counters &objectives, weapon_ring_state &ring);

} // namespace darker::game
