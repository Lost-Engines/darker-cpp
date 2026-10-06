#pragma once

#include <cstdint>
#include "game/object_definition.h"
#include "game/projectile_placement.h"
#include "game/projectile_pool.h"

namespace darker::game {

void advance_direct_projectile(projectile_placement &state, object_definition const &definition, std::uint16_t frame_step);

// True requests expiry handling; it does not unlink or recycle the object.
bool update_projectile_deadline(projectile &record, std::uint16_t clock);

} // namespace darker::game
