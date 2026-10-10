#pragma once

#include <cstdint>
#include <optional>
#include "game/object_definition.h"
#include "game/object_pose.h"

namespace darker::game {

uint16_t dual_launch_separation(object_pose const &detonator, object_pose const &capsule) noexcept;
std::optional<impact_strength> dual_launch_impact(object_pose const &origin, object_pose const &victim, bool airborne) noexcept;

} // namespace darker::game
