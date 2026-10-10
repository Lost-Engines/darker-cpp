#include "game/aircraft_spawning.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include <utility>
#include "game/random.h"

namespace darker::game {

void prepare_delphi_aircraft_sites(aircraft_spawning &state, city_map &cells) {
  /// 8EEE reserves animation slot zero for HQ and opens the occupied warehouse platforms in slots one through eight
  state.platforms.fill(0);
  state.halon = false;
  state.sites.assign(delphi_aircraft_sites.begin(),delphi_aircraft_sites.end());
  for(size_t i{0}; i < delphi_aircraft_sites.size(); ++i) {
    auto const site{delphi_aircraft_sites[i]};
    auto &cell{cells[(site >> 8)*128 + (site & 127)]};
    ++cell.state;
    if(cell.state & 0x40) state.platforms[i] = -1;
  }
}

size_t prepare_halon_aircraft_sites(aircraft_spawning &state, city_map &cells, std::span<std::byte const> const program) {
  /// C178 retains the scripted list and C87E marks its state bytes without clearing existing state bits
  state.halon = true;
  state.sites.clear();
  size_t cursor{0};
  while(cursor < program.size()) {
    auto const column{std::to_integer<uint8_t>(program[cursor++])};
    if(column >= 128) {
      if(state.timers.size() < state.sites.size()) state.timers.resize(state.sites.size());
      return cursor;
    }
    if(cursor == program.size()) break;
    auto const row{std::to_integer<uint8_t>(program[cursor++])};
    if(row >= 128) throw std::invalid_argument{"Aircraft site exceeds its city map"};
    cells[row*128+column].state |= 0x80;
    state.sites.push_back(static_cast<uint16_t>(row*256+column));
  }
  throw std::invalid_argument{"Aircraft site list has no terminator"};
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
  for(auto const site : state.sites) {
    auto const column{site & 255}, row{site >> 8};
    auto const flags{cells[row*128+column].state};
    if(!(flags & 0xc0) || (flags & 0x20)) continue;
    auto &timer{state.timers[timer_index++]};
    auto const distance{std::max(static_cast<uint8_t>(magnitude(column - (player.position.column >> 8)) + magnitude(row - (player.position.row >> 8))),uint8_t{4})};
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
    actor.pose.angles = {state.halon ? state.departure_heading : uint16_t{0x8000},state.halon ? uint16_t{0x2c00} : uint16_t{0},0};
    if(state.halon) state.departure_heading = static_cast<uint16_t>(state.departure_heading+0x2800);
    actor.pose.speed = state.halon ? 300 : 100;
    actor.pose.position = {static_cast<uint16_t>(column*256+128),static_cast<uint16_t>(row*256+(state.halon ? 128 : 248)),
      static_cast<uint16_t>((state.halon ? 1160 : 100) - bank.header_at(actor.parameters.model_token).height)};
    actor.previous_position = actor.pose.position;
    actor.current_cell = actor.target_token = site;
    actor.parameters.update_entry = object_update::departing_aircraft;
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
  advance_actor_speed(actor.pose,definition.base_speed,definition.role_data.craft().acceleration,definition.role_data.craft().deceleration,frame_step);
  steer_actor(actor.pose,actor.attitude,{.response{actor.parameters.angular_response},.bank_response{actor.parameters.motion.bank_response},
    .bank_limit{actor.parameters.motion.bank_limit},.turn_response{actor.parameters.motion.turn_response}},0x0c00,0,frame_step);
  if(std::bit_cast<int16_t>(static_cast<uint16_t>(clock - actor.script.deadline)) >= 0) {
    actor.parameters.update_entry = object_update::surface_actor;
    actor.flags &= 0xef;
  }
}

} // namespace darker::game
