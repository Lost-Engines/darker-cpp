#pragma once

#include <cstdint>

namespace darker::game {

struct game_clock {
  std::uint16_t ticks{0};
  std::uint16_t frame_ticks{0};
  std::uint16_t pending_changes{0};
  std::uint16_t frame_changes{0};
  std::uint8_t wraps{0};
  std::uint16_t step_limit{80};
  bool running{true};
};

void advance_game_clock(game_clock &clock, std::uint64_t interrupts) noexcept;
std::uint16_t consume_game_frame(game_clock &clock) noexcept;

} // namespace darker::game
