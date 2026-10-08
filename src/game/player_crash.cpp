#include "game/player_crash.h"
#include <bit>

namespace darker::game {

bool start_player_crash(object_pose &pose, player_crash_state &state, std::uint16_t const clock) noexcept {
  /// 6F4F installs the crash lifecycle once, retaining unrelated flags and the original wrapping deadline
  if(state.flags & 0x20) return false;
  state.crashing = true;
  state.flags |= 0x28;
  state.deadline = static_cast<std::uint16_t>(clock + 1536);
  pose.speed = 0;
  pose.angles[1] = 0x0205;
  return true;
}

bool player_crash_finished(player_crash_state const &state, std::uint16_t const clock) noexcept {
  /// 79E5 expires the player strictly after its wrapping deadline; 798C then requests outcome 2
  return state.crashing && std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(state.deadline - clock)) < 0;
}

void advance_player_crash(object_pose &pose, std::uint16_t const frame_step) noexcept {
  /// 6EF7 rotates the destroyed craft and moves its pitch towards EC00 with the original signed scalar step
  auto const turn{static_cast<std::uint16_t>(frame_step << 5)};
  pose.angles[0] = static_cast<std::uint16_t>(pose.angles[0] + turn);
  int const target{-5120};
  int const previous{std::bit_cast<std::int16_t>(pose.angles[1])};
  auto const next{std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(previous + (previous < target ? turn >> 3 : -(turn >> 3))))};
  pose.angles[1] = static_cast<std::uint16_t>(previous < target ? (next < target ? next : target) : (next < target ? target : next));
}

} // namespace darker::game
