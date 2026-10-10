#pragma once

#include <cstdint>
#include "game/time.h"

namespace darker::game {

bool advance_object_deadline(uint8_t &flags, clock_tick &deadline, uint8_t &fade, uint16_t altitude, clock_tick clock) noexcept;

} // namespace darker::game
