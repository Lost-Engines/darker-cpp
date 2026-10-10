#pragma once

#include <cstdint>
#include "game/object_pose.h"
#include "game/time.h"

namespace darker::game {

struct player_crash_state {
  uint8_t flags{0};
  clock_tick deadline{0};
  bool crashing{false};
};

bool start_player_crash(object_pose &pose, player_crash_state &state, clock_tick clock) noexcept;
bool player_crash_finished(player_crash_state const &state, clock_tick clock) noexcept;
void advance_player_crash(object_pose &pose, game_duration frame_step) noexcept;

} // namespace darker::game
