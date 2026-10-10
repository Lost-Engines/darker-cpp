#include "game/skimma_flight.h"
#include <algorithm>
#include <bit>
#include "game/angular_motion.h"
#include "game/flight_attitude.h"
#include "game/flight_motion.h"
#include "maths/sine_table.h"

namespace darker::game {
namespace {

int16_t word(int const value) noexcept {
  /// Retain the original word boundaries before signed coupling arithmetic
  return std::bit_cast<int16_t>(static_cast<uint16_t>(value));
}

int16_t sine(uint16_t const angle) noexcept {
  /// Flight uses the unrounded angle index
  return maths::original_sine[angle >> 6];
}

int16_t high_product(int16_t const left, int16_t const right) noexcept {
  /// Preserve the signed high word used by the original flight coupling
  return static_cast<int16_t>((left * right) >> 16);
}

} // anonymous namespace

void advance_skimma_flight(skimma_flight_state &state, skimma_flight_parameters const parameters,
  skimma_flight_input const input, game_duration frame_step) noexcept {
  /// 8108 combines shield recharge, speed-sensitive steering, pitch limits and the Skimma's forward/vertical drive
  recharge_skimma_shield(state.damage, frame_step);
  auto &angles{state.pose.angles};
  unsigned int const deficit{state.pose.speed < 2047 ? 2047u - state.pose.speed : 0};
  auto const gain{static_cast<uint16_t>((deficit * deficit) >> 7)};
  auto const bank_drive{word((word(gain) * word(input.bank_drive * 2)) >> 15)};
  auto const bank_response{calculate_driven_angular_response(state.damage.rotation.turn, parameters.angular_response, static_cast<uint16_t>(bank_drive), frame_step)};
  frame_step = bank_response.frame_step;
  state.damage.rotation.turn = bank_response.rate;
  auto bank{word(angles.roll + bank_response.angle_delta)};
  auto const half_bank_delta{word(bank_response.angle_delta) >> 1};
  auto const middle{word(bank - half_bank_delta)};
  int const sign{middle < 0 ? -1 : 0};
  int const magnitude{static_cast<uint16_t>(middle ^ sign)};
  int const threshold{gain >> 5};
  auto const excess{word(magnitude < threshold ? 0 : (magnitude - threshold) ^ sign)};
  auto const damping{word((excess * word(frame_step)) >> 8)};
  state.damage.rotation.turn = static_cast<uint16_t>(state.damage.rotation.turn - damping);
  bank = word(bank - high_product(damping, word(frame_step)));
  angles.roll = static_cast<uint16_t>(bank);
  auto const middle_bank{static_cast<uint16_t>(bank - half_bank_delta)};

  auto const pitch_drive{high_product(word(gain), word(input.pitch_drive * 2))};
  auto const pitch_response{calculate_driven_angular_response(state.damage.rotation.pitch, parameters.angular_response, static_cast<uint16_t>(pitch_drive), frame_step)};
  frame_step = pitch_response.frame_step;
  state.damage.rotation.pitch = pitch_response.rate;
  auto pitch_delta{project_flight_pitch(angles.pitch, middle_bank, pitch_response.angle_delta)};
  auto const tentative_pitch{word(angles.pitch + pitch_delta)};
  auto const assist_limit{static_cast<uint16_t>(std::max(0, (word(state.pose.position.height) >> 3) - 1024) + 256)};
  auto desired_pitch{word(0x3800 - state.pose.position.height)};
  bool force_assist{tentative_pitch > desired_pitch};
  if(!force_assist) {
    desired_pitch = -4096;
    force_assist = tentative_pitch <= desired_pitch;
  }
  angular_response assist{};
  if(force_assist || assist_limit >= state.pose.speed) {
    auto const response{static_cast<uint16_t>((force_assist ? assist_limit : assist_limit - state.pose.speed) >> 1)};
    assist = calculate_angular_response(static_cast<uint16_t>(desired_pitch - tentative_pitch), state.pitch_assist_rate, response, frame_step);
  } else {
    assist = integrate_angular_rate(state.pitch_assist_rate, 0, frame_step);
  }
  frame_step = assist.frame_step;
  state.pitch_assist_rate = assist.rate;
  pitch_delta = word(pitch_delta + assist.angle_delta);
  auto const final_pitch{static_cast<uint16_t>(angles.pitch + pitch_delta)};
  auto const middle_pitch{static_cast<uint16_t>(final_pitch - (pitch_delta >> 1))};
  auto const turn{couple_flight_turn(middle_bank, middle_pitch, pitch_response.angle_delta, frame_step)};
  auto const heading_delta{word((word(gain) * turn.heading_delta) >> 14)};
  auto const final_heading{static_cast<uint16_t>(angles.heading + heading_delta)};
  auto const middle_heading{static_cast<uint16_t>(final_heading - (heading_delta >> 1))};

  auto const movement_step{static_cast<uint16_t>((frame_step & 255) * 257)};
  auto const forward_target{static_cast<uint16_t>((input.brake ? 130 : input.forward_setting) + (state.pose.speed >> 3))};
  advance_horizontal_flight(state.pose, state.horizontal_velocity, forward_target, static_cast<uint16_t>(movement_step >> 1), middle_heading, middle_pitch);
  measure_flight_speed(state.pose, state.horizontal_velocity, state.vertical_velocity);
  auto const vertical_drive{word((word(forward_target) * sine(middle_pitch)) >> 15)};
  auto const lift{high_product(turn.lift_projection, word((word(state.horizontal_velocity) >> 1) + 512))};
  auto const altitude{state.pose.position.height};
  auto const height_term{word(altitude < 256 ? 256 - altitude : altitude - 256)};
  auto const vertical_target{static_cast<uint16_t>(vertical_drive + parameters.vertical_bias + lift - (height_term >> 5))};
  advance_vertical_flight(state.pose, state.vertical_velocity, vertical_target, movement_step);
  angles.heading = final_heading;
  angles.pitch = final_pitch;
  normalise_attitude(angles);
}

} // namespace darker::game
