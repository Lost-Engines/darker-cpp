#include "game/object_deadline.h"
#include <bit>

namespace darker::game {

bool advance_object_deadline(uint8_t &flags, uint16_t &deadline, uint8_t &object_fade, uint16_t const altitude, uint16_t const clock) noexcept {
  /// 79E5–7A18 update timed flags/fade, stopping before expiry's world and mission effects
  uint8_t constexpr expiring_flag{0x20};
  uint8_t constexpr appearing_flag{0x40};
  unsigned int constexpr fade_ticks{256};
  int constexpr high_altitude_expiry{0x50};                                    // signed altitude high byte: retire objects at height 0x5000 or above
  uint16_t constexpr wrapped_negative{0x8000};
  auto mode{static_cast<uint8_t>(flags & (expiring_flag | appearing_flag))};
  if(mode == 0) return false;
  auto const delta{static_cast<uint16_t>(deadline - clock)};
  if((delta & wrapped_negative) != 0) {
    flags ^= mode;
    if((mode & expiring_flag) != 0) return true;
    object_fade = 255;
    return false;
  }
  uint8_t fade{255};
  if(delta < fade_ticks) fade = static_cast<uint8_t>(delta);
  else if(std::bit_cast<int8_t>(static_cast<uint8_t>(altitude >> 8)) >= high_altitude_expiry) {
    flags |= expiring_flag;
    mode = expiring_flag;
    deadline = static_cast<uint16_t>(clock + 255);
  }
  object_fade = (mode & expiring_flag) != 0 ? fade : static_cast<uint8_t>(~fade);
  return false;
}

} // namespace darker::game
