#include "game/aircraft_spawning.h"
#include <algorithm>
#include <bit>
#include <utility>
#include "game/random.h"

namespace darker::game {

void prepare_delphi_aircraft_sites(aircraft_spawning &state, city_map &cells) noexcept {
  /// 8EEE reserves animation slot zero for HQ and opens the occupied warehouse platforms in slots one through eight
  state.platforms.fill(0);
  for(size_t i{0}; i < delphi_aircraft_sites.size(); ++i) {
    auto const site{delphi_aircraft_sites[i]};
    auto &cell{cells[(site >> 8)*128 + (site & 127)]};
    ++cell.state;
    if(cell.state & 0x40) state.platforms[i] = -1;
  }
}

void advance_aircraft_spawning(aircraft_spawning &state, std::vector<scenario_actor> &active,
  std::vector<scenario_actor> &free, city_map const &cells, resources::geometry_bank const &bank,
  object_pose const &player, uint16_t const clock, uint16_t const frame_step, uint16_t &random_state) {
  /// 8E3F counts down occupied-site timers and launches reusable aircraft near the player
  auto const is_aircraft{[](auto const &actor){ return actor.category == actor_category::air; }};
  if(std::ranges::none_of(free,is_aircraft)) return;
  size_t timer_index{0};
  auto const magnitude{[](int const delta){
    auto const value{std::bit_cast<int8_t>(static_cast<uint8_t>(delta))};
    return static_cast<uint8_t>(value < 0 ? -value : value);
  }};
  for(auto const site : delphi_aircraft_sites) {
    auto const column{site & 255}, row{site >> 8};
    auto const flags{cells[row*128+column].state};
    if(!(flags & 0xc0) || (flags & 0x20)) continue;
    auto &timer{state.timers[timer_index++]};
    auto const distance{std::max(static_cast<uint8_t>(magnitude(column - (player.position[0] >> 8)) + magnitude(row - (player.position[1] >> 8))),uint8_t{4})};
    auto const maximum{static_cast<uint8_t>(distance + (distance >> 1))};
    if((timer >> 8) >= maximum) timer = static_cast<uint16_t>(maximum*256);
    bool const elapsed{timer < frame_step};
    timer = static_cast<uint16_t>(timer - frame_step);
    if(!elapsed) continue;
    auto const random{static_cast<uint8_t>(next_random(random_state))};
    timer = static_cast<uint16_t>(random*distance + distance*256);
    auto const available{std::ranges::find_if(free,is_aircraft)};
    if(distance >= 16 || !state.enabled || available == free.end()) continue;
    auto actor{*available};
    free.erase(available);
    actor.awareness = {};
    actor.attitude = {};
    actor.pose.angles = {0x8000,0,0};
    actor.pose.speed = 100;
    actor.pose.position = {static_cast<uint16_t>(column*256+128),static_cast<uint16_t>(row*256+248),
      static_cast<uint16_t>(100 - bank.header_at(actor.parameters.model_token).height)};
    actor.previous_position = actor.pose.position;
    actor.current_cell = actor.target_token = site;
    actor.parameters.update_entry = 0x8ddd;
    actor.flags = 0x50;
    actor.expiry = static_cast<uint16_t>(clock + 256);
    actor.script.continuation = actor.script.checkpoint;
    actor.script.deadline = static_cast<uint16_t>(clock + 1024);
    actor.script.stopped = false;
    auto const head{std::ranges::find_if(active,[](auto const &other){ return other.category >= actor_category::air; })};
    active.insert(head,std::move(actor));
  }
}

void advance_aircraft_departure(scenario_actor &actor, uint16_t const clock, uint16_t const frame_step) noexcept {
  /// 8DDD moves before steering and releases its protected take-off state after four timer pages
  actor.previous_position = actor.pose.position;
  auto const &definition{*actor.parameters.definition};
  advance_actor_speed(actor.pose,definition.base_speed,definition.role_data[0],definition.role_data[1],frame_step);
  steer_actor(actor.pose,actor.attitude,{.response{actor.parameters.angular_response},.bank_response{actor.parameters.motion[0]},
    .bank_limit{actor.parameters.motion[1]},.turn_response{actor.parameters.motion[2]}},0x0c00,0,frame_step);
  if(std::bit_cast<int16_t>(static_cast<uint16_t>(clock - actor.script.deadline)) >= 0) {
    actor.parameters.update_entry = 0x8823;
    actor.flags &= 0xef;
  }
}

} // namespace darker::game
