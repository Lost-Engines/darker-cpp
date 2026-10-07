#include "game/object_deadline.h"
#include <bit>

namespace darker::game {

bool advance_object_deadline(uint8_t &flags, uint16_t &deadline, uint8_t &object_fade, uint16_t const altitude, uint16_t const clock) noexcept {
  /// 79E5–7A18 update timed flags/fade, stopping before expiry's world and mission effects
  auto mode{static_cast<std::uint8_t>(flags & 0x60)};
  if(mode == 0) return false;
  auto const delta{static_cast<std::uint16_t>(deadline - clock)};
  if((delta & 0x8000) != 0) {
    flags ^= mode;
    if((mode & 0x20) != 0) return true;
    object_fade = 255;
    return false;
  }
  std::uint8_t fade{255};
  if(delta < 256) fade = static_cast<std::uint8_t>(delta);
  else if(std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(altitude >> 8)) >= 0x50) {
    flags |= 0x20;
    mode = 0x20;
    deadline = static_cast<std::uint16_t>(clock + 255);
  }
  object_fade = (mode & 0x20) != 0 ? fade : static_cast<std::uint8_t>(~fade);
  return false;
}

} // namespace darker::game
