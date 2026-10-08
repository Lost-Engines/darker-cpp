#pragma once

#include <cstdint>
#include "game/object_pose.h"

namespace darker::game {

struct player_crash_state {
  std::uint8_t flags{0};
  std::uint16_t deadline{0};
  bool crashing{false};
};

bool start_player_crash(object_pose &pose, player_crash_state &state, std::uint16_t clock) noexcept;
bool player_crash_finished(player_crash_state const &state, std::uint16_t clock) noexcept;
void advance_player_crash(object_pose &pose, std::uint16_t frame_step) noexcept;

} // namespace darker::game
