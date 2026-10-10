#pragma once

#include <cstdint>
#include "game/object_pose.h"

namespace darker::game {

struct player_crash_state {
  uint8_t flags{0};
  uint16_t deadline{0};
  bool crashing{false};
};

bool start_player_crash(object_pose &pose, player_crash_state &state, uint16_t clock) noexcept;
bool player_crash_finished(player_crash_state const &state, uint16_t clock) noexcept;
void advance_player_crash(object_pose &pose, uint16_t frame_step) noexcept;

} // namespace darker::game
