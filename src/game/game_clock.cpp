#include "game/game_clock.h"
#include <algorithm>

namespace darker::game {

void advance_game_clock(game_clock &clock, uint64_t interrupts) noexcept {
  /// 0BEF caps time pending since the last frame; collapse subsequent identical interrupts without changing their observed bit transitions
  if(interrupts == 0) return;
  auto const previous{clock.ticks};
  auto const pending{std::min(static_cast<uint16_t>(clock.ticks + (clock.running ? 1 : 0) - clock.frame_ticks), clock.step_limit)};
  clock.ticks = static_cast<uint16_t>(clock.frame_ticks + pending);
  clock.pending_changes |= previous ^ clock.ticks;
  --interrupts;
  if(!clock.running || interrupts == 0) return;
  auto const advances{clock.step_limit == 65535 ? interrupts : std::min<uint64_t>(interrupts, clock.step_limit - pending)};
  auto const next{static_cast<uint16_t>(clock.ticks + static_cast<uint16_t>(advances))};
  auto changes{static_cast<uint16_t>(clock.ticks ^ next)};
  if(advances >= 65536) changes = 65535;
  else {
    changes |= changes >> 1;
    changes |= changes >> 2;
    changes |= changes >> 4;
    changes |= changes >> 8;
  }
  clock.pending_changes |= changes;
  clock.ticks = next;
}

uint16_t consume_game_frame(game_clock &clock) noexcept {
  /// B0CE publishes the elapsed word, counts its carry and exchanges accumulated interrupt transitions into the frame snapshot
  auto const step{static_cast<uint16_t>(clock.ticks - clock.frame_ticks)};
  if(static_cast<unsigned int>(clock.frame_ticks) + step > 65535) ++clock.wraps;
  clock.frame_ticks = clock.ticks;
  clock.frame_changes = clock.pending_changes;
  clock.pending_changes = 0;
  return step;
}

} // namespace darker::game
