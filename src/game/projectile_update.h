#pragma once

#include <cstdint>
#include <variant>
#include "game/projectile_steering.h"

namespace darker::game {

using projectile_target = std::variant<std::monostate, projectile_placement const *, map_guidance_target>;

enum class projectile_update_result { advanced, expired };

// Called after visibility/update eligibility, with any object target already resolved.
projectile_update_result update_projectile(projectile &record, std::uint16_t clock, std::uint16_t frame_step,
  projectile_target target = {});

} // namespace darker::game
