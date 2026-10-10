#pragma once

#include <cstdint>
#include "game/object_definition.h"
#include "game/object_pose.h"
#include "game/projectile_pool.h"
#include "game/time.h"

namespace darker::game {

void advance_direct_projectile(object_pose &state, object_definition const &definition, game_duration frame_step, uint16_t speed_bonus = 0);

// true requests expiry handling; it does not unlink or recycle the object
bool update_projectile_deadline(projectile &record, clock_tick clock);

} // namespace darker::game
