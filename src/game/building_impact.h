#pragma once

#include <cstddef>
#include <cstdint>
#include "game/collision_box.h"

namespace darker::game {

bool projectile_damages_building(size_t definition_slot, collision_category category, uint8_t state, bool linked);

} // namespace darker::game
