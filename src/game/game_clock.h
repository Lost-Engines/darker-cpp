#pragma once

#include <cstdint>

namespace darker::game {

struct game_clock {
  uint16_t ticks{0};
  uint16_t frame_ticks{0};
  uint16_t pending_changes{0};
  uint16_t frame_changes{0};
  uint8_t wraps{0};
  uint16_t step_limit{80};
  bool running{true};
};

void advance_game_clock(game_clock &clock, uint64_t interrupts) noexcept;
uint16_t consume_game_frame(game_clock &clock) noexcept;

} // namespace darker::game
