#pragma once

#include <cstddef>
#include <cstdint>

namespace darker::game {

bool projectile_damages_building(size_t definition_slot, uint8_t category, uint8_t state, bool linked);

} // namespace darker::game
