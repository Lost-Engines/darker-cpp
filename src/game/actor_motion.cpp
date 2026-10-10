#include "game/actor_motion.h"
#include <algorithm>
#include <bit>
#include "game/angular_motion.h"
#include "game/flight_motion.h"

namespace darker::game {
namespace {

int16_t signed_word(int const value) noexcept {
  /// Interpret intermediate words only after preserving native truncation
  return std::bit_cast<int16_t>(static_cast<uint16_t>(value));
}

int16_t product(uint16_t const left, uint16_t const right) noexcept {
  /// IMUL followed by ADD/ADC consumes the high word of the doubled product
  return signed_word((signed_word(left) * signed_word(right)) >> 15);
}

} // anonymous namespace

uint16_t steer_actor(object_pose &pose, actor_attitude &state, actor_steering_parameters const parameters,
  uint16_t const desired_pitch, uint16_t const turn_drive, uint16_t frame_step) noexcept {
  /// 8351 approaches pitch and bank targets, then derives heading motion from the folded midpoint bank
  auto const pitch{calculate_angular_response(static_cast<uint16_t>(desired_pitch - pose.angles.pitch), state.pitch_rate, parameters.response, frame_step)};
  state.pitch_rate = pitch.rate;
  pose.angles.pitch = static_cast<uint16_t>(pose.angles.pitch + pitch.angle_delta);
  frame_step = pitch.frame_step;
  auto const drive{product(turn_drive, parameters.bank_response)};
  auto const magnitude{std::min<unsigned int>(drive < 0 ? -drive : drive, parameters.bank_limit)};
  auto const target{static_cast<uint16_t>((drive < 0 ? -static_cast<int>(magnitude) : static_cast<int>(magnitude)) * 2)};
  auto const bank{calculate_angular_response(static_cast<uint16_t>(target - pose.angles.roll), state.bank_rate, parameters.response, frame_step)};
  state.bank_rate = bank.rate;
  pose.angles.roll = static_cast<uint16_t>(pose.angles.roll + bank.angle_delta);
  frame_step = bank.frame_step;
  auto const middle{static_cast<uint16_t>(pose.angles.roll - (signed_word(bank.angle_delta) >> 1))};
  auto const turn{product(parameters.turn_response, fold_bank_angle(middle))};
  auto const delta{product(static_cast<uint16_t>((frame_step & 255) * 257), static_cast<uint16_t>(turn))};
  pose.angles.heading = static_cast<uint16_t>(pose.angles.heading + delta);
  return frame_step;
}

void advance_actor_speed(object_pose &pose, uint8_t const desired_speed, uint8_t const acceleration,
  uint8_t const deceleration, uint16_t const frame_step) noexcept {
  /// 8D75 uses definition-specific byte acceleration/deceleration before the shared 8597 displacement path
  auto const previous{signed_word(pose.speed)};
  auto const target{static_cast<int16_t>(desired_speed * 16)};
  auto const increment{static_cast<int>(((frame_step & 255) * (previous < target ? acceleration : deceleration)) >> 8)};
  auto const candidate{signed_word(previous < target ? previous + increment : previous - increment)};
  auto const speed{previous < target ? std::min(candidate, target) : std::max(candidate, target)};
  advance_speed_motion(pose, static_cast<uint16_t>(speed), frame_step);
}

} // namespace darker::game
