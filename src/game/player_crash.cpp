#include "game/player_crash.h"
#include <bit>

namespace darker::game {

bool start_player_crash(object_pose &pose, player_crash_state &state, std::uint16_t const clock) noexcept {
  /// 6F4F installs the crash lifecycle once, retaining unrelated flags and the original wrapping deadline
  std::uint8_t constexpr dead_flag{0x20};
  std::uint8_t constexpr crash_flags{0x28};
  int constexpr crash_duration_ticks{1536};
  int constexpr initial_crash_pitch{0x0205};
  if(state.flags & dead_flag) return false;
  state.crashing = true;
  state.flags |= crash_flags;
  state.deadline = static_cast<std::uint16_t>(clock + crash_duration_ticks);
  pose.speed = 0;
  pose.angles.pitch = initial_crash_pitch;
  return true;
}

bool player_crash_finished(player_crash_state const &state, std::uint16_t const clock) noexcept {
  /// 79E5 expires the player strictly after its wrapping deadline; 798C then requests outcome 2
  return state.crashing && std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(state.deadline - clock)) < 0;
}

void advance_player_crash(object_pose &pose, std::uint16_t const frame_step) noexcept {
  /// 6EF7 rotates the destroyed craft and moves its pitch towards EC00 with the original signed scalar step
  int constexpr turn_rate_shift{5};
  int constexpr pitch_rate_shift{3};
  int constexpr final_crash_pitch{-5120};
  auto const turn{static_cast<std::uint16_t>(frame_step << turn_rate_shift)};
  pose.angles.heading = static_cast<std::uint16_t>(pose.angles.heading + turn);
  int const previous{std::bit_cast<std::int16_t>(pose.angles.pitch)};
  auto const next{std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(previous + (previous < final_crash_pitch ? turn >> pitch_rate_shift : -(turn >> pitch_rate_shift))))};
  pose.angles.pitch = static_cast<std::uint16_t>(previous < final_crash_pitch ? (next < final_crash_pitch ? next : final_crash_pitch) : (next < final_crash_pitch ? final_crash_pitch : next));
}

} // namespace darker::game
