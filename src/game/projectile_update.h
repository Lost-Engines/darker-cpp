#pragma once

#include <cstdint>
#include <variant>
#include "game/projectile_steering.h"

namespace darker::game {

using projectile_target = std::variant<std::monostate, object_pose const*, map_guidance_target>;

enum class projectile_update_result { advanced, expired, detonated };

// called after visibility/update eligibility, with any object target already resolved
projectile_update_result update_projectile(projectile &record, uint16_t clock, uint16_t frame_step,
  projectile_target target = {});

} // namespace darker::game
