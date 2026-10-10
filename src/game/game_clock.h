#pragma once

#include <cstdint>
#include "game/time.h"

namespace darker::game {

struct game_clock {
  clock_tick ticks{0};
  clock_tick frame_ticks{0};
  uint16_t pending_changes{0};
  uint16_t frame_changes{0};
  uint8_t wraps{0};
  game_duration step_limit{80};
  bool running{true};
};

void advance_game_clock(game_clock &clock, uint64_t interrupts) noexcept;
game_duration consume_game_frame(game_clock &clock) noexcept;

} // namespace darker::game
