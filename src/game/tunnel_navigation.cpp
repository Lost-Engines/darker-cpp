#include "game/tunnel_navigation.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <stdexcept>
#include <utility>
#include "game/angular_motion.h"
#include "game/flight_motion.h"
#include "maths/direction.h"

namespace darker::game {

uint8_t choose_tunnel_heading(scenario_actor &actor, city_map const &cells, tunnel_network const &network) {
  /// 860D–874D choose a preferred connected-route heading, retaining door switches and the small wandering state
  if(!actor.tunnel) throw std::invalid_argument{"Underground navigation requires tunnel actor state"};
  auto &state{*actor.tunnel};
  auto const type_at{[&](uint16_t const cell){
    auto const x{cell & 127}, y{cell >> 8};
    return y < 128 ? cells[y*128+x].type : uint8_t{0};
  }};
  auto const boundary{network.crossing(type_at(actor.current_cell),{actor.current_cell,state.route})};
  auto const junction{network.junction(type_at(boundary.cell))};
  if(junction.flags & 2) {
    auto const &edge{junction.edges[1]};
    auto height{edge.heights};
    bool matches{boundary.perimeter == edge.first};
    if(!matches) {
      height >>= 4;
      matches = boundary.perimeter == edge.second;
    }
    if(matches && ((height ^ boundary.height) & 15) == 0) state.route ^= 1;
  }
  uint8_t const orientation{static_cast<uint8_t>((boundary.perimeter*2 ^ 0x80) & 0xc0)};
  auto dx{static_cast<uint8_t>(actor.target_token - boundary.cell)};
  auto dy{static_cast<uint8_t>((actor.target_token >> 8) - (boundary.cell >> 8))};
  auto const wander{[&](uint8_t const amount){
    auto phase{static_cast<uint8_t>(std::bit_cast<int8_t>(state.oscillation) >> 1)};
    if(!(state.oscillation & 1) && (state.oscillation & 0x80)) {
      state.oscillation ^= 0x82;
      phase = static_cast<uint8_t>(~phase);
    }
    return static_cast<uint8_t>(phase & 1 ? -amount : amount);
  }};
  uint8_t desired{0};
  if(dx == 0 && dy == 0) {
    uint8_t const entry{static_cast<uint8_t>(boundary.perimeter*2 ^ 0x40)};
    for(unsigned int i{0}; i < junction.edges.size(); ++i) {
      auto const &edge{junction.edges[i]};
      bool const first{static_cast<uint8_t>(edge.first*2 + entry) == 0};
      bool const second{static_cast<uint8_t>(edge.second*2 + entry) == 0};
      if(!first && !second) continue;
      auto const opposite{static_cast<uint8_t>((first ? edge.second : edge.first)*2)};
      if(static_cast<uint8_t>(opposite ^ entry) != 0x40) {
        state.progress = 0;
        actor.current_cell = boundary.cell;
        state.route = static_cast<uint8_t>((first ? 0 : 0x80) | (2 - i));
      }
      break;
    }
    desired = wander(1);
  } else {
    bool const swap_axes{std::abs(std::bit_cast<int8_t>(dx)) < std::abs(std::bit_cast<int8_t>(dy))};
    if(swap_axes) std::swap(dx,dy);
    auto turn{static_cast<uint8_t>(((swap_axes ? 0x80 : 0x40) ^ (dx & 0x80)) - orientation)};
    auto const other_turn{static_cast<uint8_t>(((swap_axes ? 0x40 : 0x80) ^ (dy & 0x80)) - orientation)};
    uint8_t amount{24};
    if(dy != 0) {
      if(!(state.oscillation & 0x81)) state.oscillation += 0x80;
      if(!(turn & 0x40)) {
        if(turn == 0) amount = 1;
        turn = other_turn;
      }
      desired = static_cast<uint8_t>(turn & 0x80 ? -amount : amount);
    } else if(turn == 0 || turn == 0x80) {
      desired = wander(turn == 0 ? 1 : 24);
    } else {
      constexpr std::array<int,4> steps{-256,1,256,-1};
      auto const next{static_cast<uint16_t>(boundary.cell + steps[static_cast<uint8_t>(turn + orientation) >> 6])};
      auto const door{static_cast<uint8_t>(type_at(next) - 46)};
      if(door < 4) {
        state.progress = 0x113;
        state.route = static_cast<uint8_t>((door == 1 || door == 2 ? 0x80 : 0) | (orientation == 0 || orientation == 0xc0 ? 2 : 0));
        actor.current_cell = next;
      }
      desired = static_cast<uint8_t>(turn & 0x80 ? -24 : 24);
    }
  }
  auto const heading{static_cast<uint8_t>(((actor.pose.angles[0] >> 8) + 0x20) & 0xc0)};
  return static_cast<uint8_t>((desired - heading)*2 ^ 0x80);
}

void advance_tunnel_actor(scenario_actor &actor, object_pose const &player, std::span<scenario_actor> const active,
  city_map const &cells, tunnel_network const &network, uint16_t frame_step) {
  /// 8609 follows the tunnel route after script execution, then steers directly and slows for nearby aircraft
  if(actor.parameters.update_entry != 0x8609 || !actor.tunnel) throw std::invalid_argument{"Actor requires the underground movement callback"};
  actor.previous_position = actor.pose.position;
  auto const &definition{*actor.parameters.definition};
  advance_actor_awareness(actor.awareness,actor.pose,player,
    {.decay{actor.behaviour[2]},.rise{actor.behaviour[3]},.strength{actor.behaviour[4]},.cooldown_shift{definition.role_data[3]}},frame_step);
  auto const preferred{choose_tunnel_heading(actor,cells,network)};
  auto const path{network.trace(cells,{actor.current_cell,actor.tunnel->route},actor.pose.position,152,preferred)};
  auto heading{actor.pose.angles[0]}, pitch{actor.pose.angles[1]};
  if(path) {
    actor.current_cell = path->connection.cell;
    actor.tunnel->route = path->connection.route;
    actor.tunnel->progress = path->progress;
    auto target{path->target};
    auto const dx{static_cast<uint16_t>(player.position[0] - target[0])};
    auto const dy{static_cast<uint16_t>(player.position[1] - target[1])};
    if(static_cast<uint8_t>((dx >> 8) + 1) < 2 && static_cast<uint8_t>((dy >> 8) + 1) < 2) {
      auto const amount{std::max(dx & 255,static_cast<uint16_t>(dy + 256) >> 1)};
      target[2] = static_cast<uint16_t>(std::min(512,384 + amount));
    }
    auto const direction{maths::object_target_direction(actor.pose.position,target)};
    heading = direction.heading;
    pitch = direction.pitch;
  }
  auto const pitch_motion{calculate_angular_response(static_cast<uint16_t>(pitch - actor.pose.angles[1]),
    actor.attitude.pitch_rate,actor.parameters.angular_response,frame_step)};
  actor.attitude.pitch_rate = pitch_motion.rate;
  actor.pose.angles[1] = static_cast<uint16_t>(actor.pose.angles[1] + pitch_motion.angle_delta);
  auto const heading_motion{calculate_angular_response(static_cast<uint16_t>(heading - actor.pose.angles[0]),
    actor.attitude.bank_rate,actor.parameters.angular_response,pitch_motion.frame_step)};
  actor.attitude.bank_rate = heading_motion.rate;
  actor.pose.angles[0] = static_cast<uint16_t>(actor.pose.angles[0] + heading_motion.angle_delta);
  actor.pose.angles[2] = static_cast<uint16_t>(-heading_motion.rate);
  frame_step = heading_motion.frame_step;
  uint16_t target_speed{static_cast<uint16_t>(actor.awareness.level == 0 ? 284 : 512)};
  for(auto &neighbour : active) {
    if(neighbour.category != actor_category::air || neighbour.index == actor.index) continue;
    auto const dx{static_cast<uint16_t>(neighbour.pose.position[0] - actor.pose.position[0])};
    auto const dy{static_cast<uint16_t>(neighbour.pose.position[1] - actor.pose.position[1])};
    if(static_cast<uint16_t>(dx + 256) >= 512 || static_cast<uint16_t>(dy + 256) >= 512) continue;
    auto const direction{static_cast<uint16_t>((maths::direction_index(dx,dy) << 5) - actor.pose.angles[0])};
    if(static_cast<uint8_t>((direction >> 8) - 0x6c) >= 0x28) continue;
    target_speed = 256;
    if(neighbour.awareness.level == 0) neighbour.awareness.level = 0x400;
  }
  auto const previous{std::bit_cast<int16_t>(actor.pose.speed)};
  auto const target{static_cast<int16_t>(target_speed)};
  auto const candidate{std::bit_cast<int16_t>(static_cast<uint16_t>(previous < target ? previous + frame_step : previous - frame_step))};
  advance_speed_motion(actor.pose,static_cast<uint16_t>(previous < target ? std::min(candidate,target) : std::max(candidate,target)),frame_step);
}

} // namespace darker::game
