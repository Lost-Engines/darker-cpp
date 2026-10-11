#pragma once

#include <cstdint>

namespace darker::presentation {

struct quit_confirmation {
  bool active{false};
  uint8_t started{0};

  void toggle(uint16_t const clock) noexcept {
    /// B7CE toggles the prompt and restarts its flash without pausing flight.
    started = static_cast<uint8_t>(clock);
    active = !active;
  }

  bool visible(uint16_t const clock) const noexcept {
    /// 3E9F: on for 170 ticks of each wrapping 256-tick period.
    uint8_t constexpr lit_ticks{170};
    return active && static_cast<uint8_t>(clock - started) < lit_ticks;
  }
};

} // namespace darker::presentation
