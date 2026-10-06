#include "game/caero_flight.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include "game/angular_motion.h"
#include "game/beacon_light.h"
#include "game/flight_attitude.h"
#include "game/flight_motion.h"
#include "maths/sine_table.h"

namespace darker::game {
namespace {

std::int16_t word(int const value) noexcept {
  /// Retain each original word boundary before signed arithmetic
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

std::int16_t sine(std::uint16_t const angle) noexcept {
  /// Flight indexes the extended sine table without the renderer's rounding bias
  return maths::original_sine[angle >> 6];
}

std::int16_t high_product(std::int16_t const left, std::int16_t const right) noexcept {
  /// Preserve the signed high word consumed by the craft's coupling terms
  return static_cast<std::int16_t>((left * right) >> 16);
}

} // namespace

bool activate_caero_boost(caero_flight_state &state) noexcept {
  /// B963 spends one complete boost pip and replaces only the high byte of the active drive duration
  if((state.energy.boost >> 8) < 32) return false;
  state.energy.boost -= 32 * 256;
  state.active_boost = static_cast<std::uint16_t>((state.active_boost & 255) | 0x3800);
  return true;
}

void advance_caero_flight(caero_flight_state &state, caero_flight_parameters const parameters,
  caero_flight_input const input, std::uint16_t frame_step, std::span<city_cell const, 128 * 128> const cells) {
  /// 7E7F/7EB6 order startup, steering, energy spending, movement, beacon sampling and charging within one callback
  if(!state.flying) {
    if((state.active_boost >> 8) == 0) {
      // Retail hangar charging continues with the engine switched off; see docs/hangar_launch.md.
      state.startup_energy = static_cast<std::uint16_t>(state.startup_energy + frame_step * 7);
      if((state.startup_energy >> 8) >= 0x50) state.startup_energy = static_cast<std::uint16_t>((state.startup_energy & 255) | 0x5000);
      state.energy.boost = static_cast<std::uint16_t>((state.energy.boost & 255) | (((state.startup_energy >> 8) * 2 & 0xe0) << 8));
      return;
    }
    state.flying = true;
  }
  repair_caero_damage(state.damage, state.repair_phase, frame_step);
  auto &angles{state.pose.angles};
  auto const bank_response{calculate_driven_angular_response(state.damage.rotation.turn, parameters.angular_response, input.bank_drive, frame_step)};
  frame_step = bank_response.frame_step;
  state.damage.rotation.turn = bank_response.rate;
  angles[2] = static_cast<std::uint16_t>(angles[2] + bank_response.angle_delta);
  auto const middle_bank{static_cast<std::uint16_t>(angles[2] - (word(bank_response.angle_delta) >> 1))};

  angular_response pitch_response{};
  if(input.altitude_hold) {
    auto const height_error{word((word(parameters.height_reference) >> 2) + parameters.desired_height - state.pose.position[2])};
    auto const desired_pitch{word(std::clamp<int>(height_error, -512, 512) * 8)};
    pitch_response = calculate_angular_response(static_cast<std::uint16_t>(desired_pitch - angles[1]), state.damage.rotation.pitch, parameters.angular_response, frame_step);
  } else {
    pitch_response = calculate_driven_angular_response(state.damage.rotation.pitch, parameters.angular_response, input.pitch_drive, frame_step);
  }
  frame_step = pitch_response.frame_step;
  state.damage.rotation.pitch = pitch_response.rate;
  auto pitch_delta{project_flight_pitch(angles[1], middle_bank, pitch_response.angle_delta)};
  auto const tentative_pitch{word(angles[1] + pitch_delta)};
  angular_response assist{};
  if(tentative_pitch > -4096 && state.pose.speed <= 300) {
    assist = calculate_angular_response(static_cast<std::uint16_t>(-4096 - tentative_pitch), state.pitch_assist_rate,
      static_cast<std::uint16_t>((300 - state.pose.speed) >> 1), frame_step);
  } else {
    assist = integrate_angular_rate(state.pitch_assist_rate, 0, frame_step);
  }
  frame_step = assist.frame_step;
  state.pitch_assist_rate = assist.rate;
  pitch_delta = word(pitch_delta + assist.angle_delta);
  auto const final_pitch{static_cast<std::uint16_t>(angles[1] + pitch_delta)};
  auto const middle_pitch{static_cast<std::uint16_t>(final_pitch - (pitch_delta >> 1))};

  auto const turn{couple_flight_turn(middle_bank, middle_pitch, pitch_response.angle_delta, frame_step)};
  auto const heading_delta{turn.heading_delta};
  auto const final_heading{static_cast<std::uint16_t>(angles[0] + heading_delta)};
  auto const middle_heading{static_cast<std::uint16_t>(final_heading - (heading_delta >> 1))};

  auto const accounting_step{static_cast<std::uint16_t>(((frame_step & 255) * 257) >> 1)};
  auto const drive_step{static_cast<std::uint16_t>(accounting_step >> 1)};
  if(drive_step == 0) throw std::domain_error{"Caero flight timestep produces an original zero drive divisor"};
  auto const spent_buffer{std::min(state.energy.buffer, static_cast<std::uint16_t>(frame_step << 4))};
  state.energy.buffer -= spent_buffer;
  auto const spent_boost{std::min(state.active_boost, drive_step)};
  state.active_boost -= spent_boost;
  auto const drive{static_cast<std::uint16_t>(spent_boost * 2 + (spent_buffer >> 1))};
  auto const demand{static_cast<std::uint32_t>(parameters.drive_multiplier) * drive / drive_step};
  if(demand > 65535) throw std::domain_error{"Caero forward demand exceeds the original quotient"};
  auto const forward_target{static_cast<std::uint16_t>(demand + state.forward_bias)};
  advance_horizontal_flight(state.pose, state.horizontal_velocity, forward_target, accounting_step, middle_heading, middle_pitch);
  auto const incoming{input.engine_flags == 1 ? beacon_light(cells, state.pose.position, {state.pose.fractions[0], state.pose.fractions[1]}) : std::uint16_t{0}};
  measure_flight_speed(state.pose, state.horizontal_velocity, state.vertical_velocity);
  state.forward_bias = static_cast<std::uint16_t>((state.pose.speed + (input.brake ? 0 : incoming >> 1)) >> 3);
  charge_caero_energy(state.energy, incoming, input.engine_flags, accounting_step, input.boost_cheat);

  auto const vertical_drive{word((word(forward_target) * sine(middle_pitch)) >> 15)};
  auto const absolute_projection{word(turn.lift_projection < 0 ? -turn.lift_projection : turn.lift_projection)};
  auto lift{high_product(absolute_projection, word(incoming))};
  if(lift >= 0) {
    if(input.brake) lift = std::min<std::int16_t>(lift, 255);
    else if(word(-vertical_drive) >= 0) {
      if(input.altitude_hold) lift = std::min<std::int16_t>(lift, 255);
      else lift = static_cast<std::int16_t>(std::max(0, lift - word(-vertical_drive)));
    }
  }
  auto const altitude{state.pose.position[2]};
  auto const height_term{word(altitude < 256 ? 256 - altitude : altitude - 256)};
  auto const vertical_target{static_cast<std::uint16_t>(vertical_drive + parameters.vertical_bias + (lift >> 1) - (height_term >> 5))};
  advance_vertical_flight(state.pose, state.vertical_velocity, vertical_target, accounting_step);
  angles[0] = final_heading;
  angles[1] = final_pitch;
  normalise_attitude(angles);
}

} // namespace darker::game
