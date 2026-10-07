#pragma once

#include <cstdint>

namespace darker::game {

bool advance_object_deadline(uint8_t &flags, uint16_t &deadline, uint8_t &fade, uint16_t altitude, uint16_t clock) noexcept;

} // namespace darker::game
