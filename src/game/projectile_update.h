#pragma once

#include <cstdint>
#include "game/projectile_pool.h"

namespace darker::game {

enum class projectile_update_result { advanced, expired };

// Called after visibility/update eligibility, with any object target already resolved.
projectile_update_result update_projectile(projectile &record, std::uint16_t clock, std::uint16_t frame_step,
  projectile_placement const *target = nullptr);

} // namespace darker::game
