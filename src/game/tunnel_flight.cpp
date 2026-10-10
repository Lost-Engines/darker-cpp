#include "game/tunnel_flight.h"
#include <algorithm>
#include <bit>
#include <cstdlib>
#include "game/angular_motion.h"
#include "game/city_map.h"
#include "game/flight_motion.h"
#include "maths/direction.h"
#include "maths/angle.h"

namespace darker::game {
namespace {

int16_t word(int const value) noexcept {
  /// Preserve signed word intermediates in steering and speed penalties
  return std::bit_cast<int16_t>(static_cast<uint16_t>(value));
}

uint16_t approach(uint16_t const previous, uint16_t const target, uint16_t const step) noexcept {
  /// 7CDB advances towards a signed target with native word wrapping before the clamp
  auto const candidate{word(word(previous) < word(target) ? previous + step : previous - step)};
  return static_cast<uint16_t>(word(previous) < word(target) ? std::min(candidate, word(target)) : std::max(candidate, word(target)));
}

uint16_t smooth(uint16_t const previous, uint16_t const drive, game_duration const frame_step) noexcept {
  /// D86B clamps steering demand and approaches growing demand twice as quickly as falling demand
  auto const magnitude{std::min(std::abs(word(drive)), 3072)};
  auto const old_magnitude{static_cast<uint16_t>(word(previous) < 0 ? -previous : previous)};
  auto const step{static_cast<uint16_t>(frame_step * (magnitude > old_magnitude ? 8 : 4))};
  return approach(previous, static_cast<uint16_t>(word(drive) < 0 ? -magnitude : magnitude), step);
}

struct route_response {
  uint16_t error;
  uint16_t gain;
  uint16_t resistance;
};

route_response response(uint16_t const drive, uint16_t const route_error) noexcept {
  /// D829 blends manual steering with route alignment and reduces gain as their disagreement grows
  auto demand{word(drive * 5)};
  auto error{word(route_error)};
  if(error < 0) {
    demand = word(-demand);
    error = word(-error);
  }
  if(static_cast<uint16_t>(error) >= 0x2800) error = 0x2800;
  uint16_t resistance{0}, gain{320};
  if(error > demand) {
    demand >>= 3;
    error = word(error - demand);
    demand >>= 3;
    error = word(error + demand);
    resistance = static_cast<uint16_t>(error);
    auto const reduction{word(error) >> 5};
    gain = static_cast<uint16_t>(gain < static_cast<uint16_t>(reduction) ? 0 : gain - reduction);
  }
  return {static_cast<uint16_t>((word(drive) >> 2) + route_error), gain, resistance};
}

} // anonymous namespace

void advance_tunnel_flight(caero_flight_state &craft, tunnel_flight_state &state, tunnel_flight_input const input,
  game_duration frame_step, city_map const &cells, tunnel_network const &network) {
  /// D510 repairs damage, recharges connected flight and separates low-speed aiming from the route-following attitude
  auto &angles{craft.pose.angles};
  repair_caero_damage(craft.damage, craft.repair_phase, frame_step);
  if(!(state.connection.route & 0x40)) {
    craft.energy.reserve = static_cast<uint16_t>(craft.energy.reserve + frame_step * 40);
    if((craft.energy.reserve >> 8) >= 0xd0) craft.energy.reserve = 0xcfff;
    craft.energy.reserve_display = static_cast<uint8_t>(craft.energy.reserve >> 12);
    craft.energy.boost = std::min(static_cast<uint16_t>(craft.energy.boost + frame_step * 8), uint16_t{0x1fff});
  }
  bool const aiming{craft.pose.speed <= 180 && (input.forward_setting >> 8) != 0 && !(state.connection.route & 0x40)};
  if(!aiming) {
    bool const leaving{state.aiming};
    state.aiming = false;
    if(leaving && !(state.connection.route & 0x40)) {
      frame_step = static_cast<uint16_t>((frame_step & 0xff00) | input.cell_collision_marker);
      auto const cell{state.connection.cell};
      auto const type{cells.at(packed_cell_reference{cell}.index()).type};
      auto const edge{network.segment(type, state.connection.route)};
      auto phase{static_cast<uint8_t>((edge.heading >> 1) + (angles.heading >> 8))};
      if(edge.first < 0x60 || static_cast<uint8_t>(0xa0 - edge.first) > edge.second) phase = static_cast<uint8_t>(~phase);
      state.connection.route = network.direction(type, state.connection.route, angles.heading);
      if(static_cast<uint8_t>(phase * 2 + 0x98) >= 0x30) state.connection.route |= 0x40;
    }
    advance_tunnel_motion(craft, state, input, frame_step, cells, network);
    return;
  }
  if(!state.aiming) {
    state.aiming = true;
    state.aim_heading = state.aim_pitch = state.aim_heading_rate = state.aim_pitch_rate = 0;
  }
  angles.heading = static_cast<uint16_t>(angles.heading - state.aim_heading);
  angles.pitch = static_cast<uint16_t>(angles.pitch - state.aim_pitch);
  auto const gain{word((228 - craft.pose.speed) * 128)};
  auto const pitch_drive{static_cast<uint16_t>((gain * word(input.pitch_drive)) >> 15)};
  auto const heading_impulse{static_cast<uint16_t>(std::clamp((gain * word(input.bank_reference)) >> 16, -4096, 4096))};
  auto const turn{integrate_angular_rate(state.aim_heading_rate, heading_impulse, frame_step)};
  state.aim_heading_rate = turn.rate;
  state.aim_heading = static_cast<uint16_t>(state.aim_heading - turn.angle_delta);
  frame_step = turn.frame_step;
  auto magnitude{std::abs(word(state.aim_pitch))};
  if(magnitude >= 256) magnitude = magnitude < 4096 ? 0 : magnitude - 4096;
  auto const limited{word(word(state.aim_pitch) < 0 ? -magnitude : magnitude)};
  auto const product{limited * word(-(frame_step & 255) * 256)};
  auto const damping{word((product >> 16) * 2 + ((static_cast<uint32_t>(product) & 0xff00) != 0 ? 1 : 0))};
  auto const tilt{calculate_driven_angular_response(state.aim_pitch_rate, input.aim_response,
    static_cast<uint16_t>(damping + pitch_drive), frame_step)};
  state.aim_pitch_rate = tilt.rate;
  state.aim_pitch = static_cast<uint16_t>(state.aim_pitch + tilt.angle_delta);
  advance_tunnel_motion(craft, state, input, tilt.frame_step, cells, network);
  angles.heading = static_cast<uint16_t>(angles.heading + state.aim_heading);
  angles.pitch = static_cast<uint16_t>(angles.pitch + state.aim_pitch);
}

void advance_tunnel_motion(caero_flight_state &craft, tunnel_flight_state &state, tunnel_flight_input const input,
  game_duration frame_step, city_map const &cells, tunnel_network const &network) {
  /// D5C9 smooths steering, follows or reacquires a route, then integrates the tunnel-specific drive and gravity
  auto &pose{craft.pose};
  auto &angles{pose.angles};
  auto const type_at{[&](uint16_t const cell){
    return cells.at(packed_cell_reference{cell}.index()).type;
  }};
  state.filtered_pitch = smooth(state.filtered_pitch, input.pitch_reference, frame_step);
  auto const sign{word(state.filtered_pitch) < 0 ? 0xffff : 0};
  if((state.filtered_pitch ^ sign) >= 0x4d8 && ((state.filtered_pitch ^ angles.pitch) & 0x8000) == 0) {
    auto const current{network.junction(type_at(state.connection.cell))};
    auto const same{[](tunnel_segment const a, tunnel_segment const b){
      return a.first == b.first && a.second == b.second;
    }};
    if(current.edges[1].first && same(current.edges[1], current.edges[2])) {
      auto const crossing{network.crossing(type_at(state.connection.cell), state.connection)};
      auto const next{network.junction(type_at(crossing.cell))};
      if(same(next.edges[1], next.edges[2])) state.connection.route = static_cast<uint8_t>((state.connection.route & 0xe0) + (sign ? 0 : 1));
    }
  }
  state.filtered_bank = smooth(state.filtered_bank, static_cast<uint16_t>(word(input.bank_reference) >> 1), frame_step);
  auto const roll{calculate_angular_response(static_cast<uint16_t>(state.filtered_bank * 2 - angles.roll),
    craft.damage.rotation.turn, input.angular_response, frame_step)};
  craft.damage.rotation.turn = roll.rate;
  angles.roll = static_cast<uint16_t>(angles.roll + roll.angle_delta);
  frame_step = roll.frame_step;
  auto const bank{static_cast<uint16_t>(std::clamp<int>(word(angles.roll), -0x700, 0x700))};
  auto const preferred{static_cast<uint8_t>(((bank >> 6) - (angles.heading >> 8)) * 2 ^ 0x80)};
  auto path{input.engine ? network.trace(cells, state.connection, pose.position, state.lookahead, preferred) : std::nullopt};
  if(input.engine && !path) {
    auto const candidate{network.reacquire(cells, state.connection, pose.position, preferred)};
    if(candidate) {
      auto const type{type_at(candidate->cell)};
      auto const edge{network.segment(type, candidate->route)};
      if(static_cast<uint8_t>((angles.heading >> 8) * 2 + edge.heading + 0xa0) < 0x40) {
        state.connection = {candidate->cell, network.direction(type, candidate->route, angles.heading)};
        path = network.trace(cells, state.connection, pose.position, state.lookahead, preferred);
        if(path) state.resistance = 0;
      }
    }
  }
  uint16_t final_heading{0}, final_pitch{0}, middle_heading{0}, middle_pitch{0}, target_resistance{3200};
  if(path) {
    state.connection = path->connection;
    state.progress = path->progress;
    state.off_route_time = 0;
    auto const desired{maths::object_target_direction(pose.position, path->target)};
    auto const heading{response(static_cast<uint16_t>(-state.filtered_bank * 2), static_cast<uint16_t>(desired.heading - angles.heading))};
    auto const turn{calculate_angular_response(heading.error, state.heading_rate, heading.gain, frame_step)};
    state.heading_rate = turn.rate;
    frame_step = turn.frame_step;
    auto const pitch_error{static_cast<uint16_t>((word(0x7fff - pose.speed * 32) * word(desired.pitch)) >> 15)};
    auto const pitch{response(static_cast<uint16_t>(state.filtered_pitch * 2), pitch_error)};
    auto const tilt{calculate_angular_response(static_cast<uint16_t>(pitch.error - angles.pitch), craft.damage.rotation.pitch, pitch.gain, frame_step)};
    craft.damage.rotation.pitch = tilt.rate;
    frame_step = tilt.frame_step;
    final_heading = static_cast<uint16_t>(angles.heading + turn.angle_delta);
    final_pitch = static_cast<uint16_t>(angles.pitch + tilt.angle_delta);
    middle_heading = static_cast<uint16_t>(final_heading - (word(turn.angle_delta) >> 1));
    middle_pitch = static_cast<uint16_t>(final_pitch - (word(tilt.angle_delta) >> 1));
    auto const first{heading.resistance}, second{static_cast<uint16_t>(pitch.resistance >> 1)};
    target_resistance = static_cast<uint16_t>(std::max(first, second) + (word(std::min(first, second)) >> 3));
    if((target_resistance >> 8) >= 10) target_resistance = 3200;
  } else {
    state.off_route_time = static_cast<uint16_t>(state.off_route_time + frame_step);
    if((state.off_route_time >> 8) >= 3) {
      state.off_route_time = static_cast<uint16_t>((state.off_route_time & 255) | 0x300);
      state.connection.route |= 0x5f;
    } else state.connection.route |= 0x40;
    auto const tilt{calculate_angular_response(static_cast<uint16_t>(state.filtered_pitch - angles.pitch), craft.damage.rotation.pitch,
      static_cast<uint16_t>(352 - (pose.speed >> 2)), frame_step)};
    craft.damage.rotation.pitch = tilt.rate;
    frame_step = tilt.frame_step;
    final_pitch = static_cast<uint16_t>(angles.pitch + tilt.angle_delta);
    middle_pitch = static_cast<uint16_t>(final_pitch - (word(tilt.angle_delta) >> 1));
    auto const turn{calculate_angular_response(static_cast<uint16_t>(-(word(state.filtered_bank) >> 1)), state.heading_rate,
      static_cast<uint16_t>(352 - (pose.speed >> 2)), frame_step)};
    state.heading_rate = turn.rate;
    frame_step = turn.frame_step;
    final_heading = static_cast<uint16_t>(angles.heading + turn.angle_delta * 2);
    middle_heading = static_cast<uint16_t>(final_heading - (word(turn.angle_delta * 2) >> 1));
  }
  state.resistance = approach(state.resistance, target_resistance, static_cast<uint16_t>(frame_step * 8));
  craft.energy.incoming_display = static_cast<uint8_t>((state.resistance <= 3200 ? 3200 - state.resistance : 0) >> 8);
  if(path && state.resistance >= static_cast<uint16_t>((688 - pose.speed) * 8)) state.connection.route |= 0x40;
  auto target_speed{input.brake ? uint16_t{60} : input.forward_setting};
  auto const cost{static_cast<uint16_t>(frame_step * 2)};
  if(craft.energy.boost < cost) {
    target_speed = static_cast<uint16_t>((static_cast<uint32_t>(craft.energy.boost) * 65536 / cost * target_speed) >> 16);
    craft.energy.boost = 0;
  } else craft.energy.boost -= cost;
  target_speed = target_speed < (state.resistance >> 8) ? 0 : static_cast<uint16_t>(target_speed - (state.resistance >> 8));
  auto const time{static_cast<uint16_t>((frame_step & 255) * 257)};
  auto vertical{word((word(target_speed * 2) * maths::angle_sine(middle_pitch)) >> 16)};
  advance_horizontal_flight(pose, craft.horizontal_velocity, target_speed, static_cast<uint16_t>(time >> 1), middle_heading, middle_pitch);
  measure_flight_speed(pose, craft.horizontal_velocity, craft.vertical_velocity);
  if(state.connection.route & 0x40) vertical = word(vertical - 25 + (craft.horizontal_velocity >> 5));
  advance_vertical_flight(pose, craft.vertical_velocity, static_cast<uint16_t>(vertical), time);
  angles.heading = final_heading;
  angles.pitch = final_pitch;
  normalise_attitude(angles);
}

} // namespace darker::game
