#include "game/hangar.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include "game/actor_motion.h"
#include "game/flight_motion.h"
#include "game/object_definitions.h"
#include "maths/direction.h"
#include "maths/world_coordinates.h"
#include "game/player_flags.h"

namespace darker::game {
namespace {

int constexpr entrance_model_type{17};
int constexpr last_hangar_model_type{24};
int constexpr berth_row_offset{152};
uint16_t constexpr maximum_door_extension{0xe800};

size_t site_index(uint16_t const site) {
  /// Return sites use the world's packed byte address, with twice the column in its low byte
  auto const column{(site & 255) >> 1};
  auto const row{site >> 8};
  if((site & 1) || row == 0 || row >= city_map_size.row - 1) throw std::invalid_argument{"Invalid Caero hangar site"};
  return city_cell_index(column, row);
}

void toggle_hangar(player_flight &player, city_map &cells, size_t const centre) {
  /// C6D2 toggles the landed flag and the gate, approach-light and interior alternate states together
  set_player_flag(player.lifecycle.flags, player_flag::ground_protection, !has_player_flag(player.lifecycle.flags, player_flag::ground_protection));
  for(auto const index : {centre - city_map_size.column, centre, centre + city_map_size.column}) cells[index].state ^= 0x80;
}

} // anonymous namespace

void initialise_caero_hangar(player_flight &player, city_map &cells, hangar_state &hangar, int16_t const model_height) {
  /// BD34–BD98 places the Caero at the type-17 return sites used by the Delphi campaign
  auto const centre{site_index(hangar.return_site)};
  if(cells[centre].type != entrance_model_type) throw std::invalid_argument{"Caero launch requires a campaign type-17 hangar"};
  int constexpr initial_thrust_energy{0x6000};
  int constexpr initial_height_offset{-104};
  int constexpr launch_pitch{0x0a20};
  player = {};
  // outer startup 3D50–3D52 supplies thrust energy independently of the visible boost-cell reserve
  std::get<caero_flight_state>(player.craft).energy.buffer = initial_thrust_energy;
  player.pose() = {
    .position{
      .column{static_cast<uint16_t>((centre % city_map_size.column) * 256 + 128)},
      .row{static_cast<uint16_t>((centre / city_map_size.column) * 256 + berth_row_offset)},
      .height{static_cast<uint16_t>(initial_height_offset - model_height)}
    },
    .angles{
      .heading{0},
      .pitch{launch_pitch},
      .roll{0}
    },
  };
  hangar.extension = 0;
  hangar.sound_level = 0;
  toggle_hangar(player, cells, centre);
}

void advance_hangar_departure(player_flight &player, city_map &cells, hangar_state &hangar, game_duration const frame_step) {
  /// C5F9 opens the gate inside its cell, retracts it over the approach lights and closes the site after departure
  if(!has_player_flag(player.lifecycle.flags, player_flag::ground_protection) || player.lifecycle.crashing) return;
  if(hangar.returning == hangar_return_phase::settling || hangar.returning == hangar_return_phase::complete) return;
  auto const centre{site_index(hangar.return_site)};
  auto const &position{player.pose().position};
  auto const column{static_cast<unsigned int>(position.column >> 8)};
  auto const row{static_cast<unsigned int>(position.row >> 8)};
  auto const type{column < city_map_size.column && row < city_map_size.row ? cells[city_cell_index(column, row)].type : 255};
  if(type < entrance_model_type || type > last_hangar_model_type) {
    if(hangar.returning == hangar_return_phase::approaching) return;
    hangar.extension = 0;
    hangar.sound_level = 0;
    toggle_hangar(player, cells, centre);
    if(hangar.next_return_site != 0) hangar.return_site = hangar.next_return_site;
    return;
  }
  if(type & 1) {
    auto const delta{static_cast<uint16_t>(frame_step << 6)};
    auto const sum{static_cast<unsigned int>(hangar.extension) + delta};
    if(sum >= maximum_door_extension) {
      hangar.extension = maximum_door_extension;
      return;
    }
    hangar.extension = static_cast<uint16_t>(sum);
  } else {
    auto const distance{horizontal_distance(position, {static_cast<uint16_t>((centre % city_map_size.column) * 256 + 128),
      static_cast<uint16_t>((centre / city_map_size.column) * 256 + berth_row_offset), 0})};
    auto value{static_cast<uint16_t>(152 - distance)};
    if(distance != 0 && distance <= 152) value = 65535;
    unsigned int const sum{value + 232u};
    hangar.extension = sum > 65535 ? static_cast<uint16_t>((sum & 255) * 257) : 0;
  }
  hangar.sound_level = static_cast<uint8_t>((hangar.extension & 255) | (hangar.extension >> 8));
}

bool begin_hangar_return(player_flight &player, city_map &cells, hangar_state &hangar, bool const objectives_complete) {
  /// C670 admits the type-17 approach only after objectives clear and position/attitude fit the original capture window
  int constexpr capture_height_limit_cells{9};
  int constexpr alignment_half_width{7};
  int constexpr alignment_window_width{14};
  int constexpr minimum_capture_distance{0x260};
  int constexpr capture_distance_range{256};
  if(!objectives_complete || player_actions_blocked(player.lifecycle.flags) || hangar.returning != hangar_return_phase::none) return false;
  auto const centre{site_index(hangar.return_site)};
  if(cells[centre].type != entrance_model_type) throw std::invalid_argument{"Return capture currently requires a type-17 hangar"};
  auto const &pose{player.pose()};
  if((pose.position.height >> 8) >= capture_height_limit_cells) return false;
  auto const aligned{[](uint16_t const error){
    return static_cast<uint8_t>((error >> 8) + alignment_half_width) < alignment_window_width;
  }};
  if(!aligned(pose.angles.roll)) return false;
  maths::world_position const target{
    .column{static_cast<uint16_t>((centre % city_map_size.column) * 256 + 128)},
    .row{static_cast<uint16_t>((centre / city_map_size.column) * 256 + berth_row_offset)},
    .height{256}
  };
  auto const distance{horizontal_distance(pose.position, target)};
  if(static_cast<uint16_t>(distance - minimum_capture_distance) >= capture_distance_range) return false;
  auto const direction{maths::object_target_direction(pose.position, target)};
  if(!aligned(static_cast<uint16_t>(direction.pitch - pose.angles.pitch))
    || !aligned(static_cast<uint16_t>(direction.heading - pose.angles.heading))
    || !aligned(static_cast<uint16_t>(0x8000 - direction.heading))) return false;
  hangar.returning = hangar_return_phase::approaching;
  toggle_hangar(player, cells, centre);
  return true;
}

void advance_hangar_return(player_flight &player, hangar_state &hangar, game_duration frame_step, clock_tick const clock) {
  /// 7CEF/7D32 steer through the approach and inner berth, then turn in place until the original completion deadline
  int constexpr tunnel_berth_row_offset{144};
  int constexpr surface_berth_row_offset{216};
  int constexpr tunnel_berth_height{1640};
  int constexpr surface_berth_height{220};
  int constexpr arrival_position_tolerance{7};
  int constexpr arrival_position_window{2 * arrival_position_tolerance + 1};
  int constexpr approach_speed_bias{80};
  unsigned int constexpr maximum_approach_speed{280};
  int constexpr settling_duration_ticks{0x7ff};
  int constexpr door_closing_delay_ticks{1024};
  auto &craft{std::get<caero_flight_state>(player.craft)};
  auto &pose{craft.pose};
  auto const &definition{original_object_definitions[player.tunnel ? 28 : 25]};
  actor_attitude attitude{craft.damage.rotation.pitch, craft.damage.rotation.turn};
  actor_steering_parameters parameters{
    .response{static_cast<uint16_t>(definition.angular_seed * 8)},
    .bank_response{static_cast<uint16_t>(definition.motion_seeds.bank_response * 256)},
    .bank_limit{static_cast<uint16_t>(definition.motion_seeds.bank_limit * 64)},
    .turn_response{static_cast<uint16_t>(definition.motion_seeds.turn_response * 256)},
  };
  auto const steer{[&](uint16_t const pitch, uint16_t const drive){
    frame_step = steer_actor(pose, attitude, parameters, pitch, drive, frame_step);
    craft.damage.rotation = {attitude.pitch_rate, attitude.bank_rate};
  }};
  if(hangar.returning == hangar_return_phase::approaching) {
    auto const centre{site_index(hangar.return_site)};
    maths::world_position target{
      .column{static_cast<uint16_t>((centre % city_map_size.column) * 256 + 128)},
      .row{static_cast<uint16_t>((centre / city_map_size.column) * 256 + (player.tunnel ? tunnel_berth_row_offset : surface_berth_row_offset))},
      .height{static_cast<uint16_t>(player.tunnel ? tunnel_berth_height : surface_berth_height)}
    };
    auto const distance{horizontal_distance(pose.position, target)};
    auto const approach{[&](maths::world_position const &point){
      bool const close{static_cast<uint16_t>(point.column - pose.position.column + arrival_position_tolerance) < arrival_position_window
        && static_cast<uint16_t>(point.row - pose.position.row + arrival_position_tolerance) < arrival_position_window};
      if(close) return true;
      auto const direction{maths::object_target_direction(pose.position, point)};
      auto const difference{static_cast<uint16_t>(direction.heading - pose.angles.heading)};
      if(((difference ^ static_cast<uint16_t>(difference << 1)) & 0x8000) != 0) return true;
      steer(direction.pitch, difference);
      return false;
    }};
    auto outer{target};
    outer[1] -= 256;
    bool arrived{false};
    if(player.tunnel) arrived = approach(target);
    else if(approach(outer)) {
      target.height = static_cast<uint16_t>(std::min<int>(static_cast<int>(distance >> 2) - 40, 41));
      arrived = approach(target);
    }
    if(!arrived) {
      auto const speed{static_cast<int16_t>(std::min<unsigned int>(static_cast<uint16_t>(distance + approach_speed_bias), maximum_approach_speed))};
      auto const old{std::bit_cast<int16_t>(pose.speed)};
      auto const amount{static_cast<uint16_t>(frame_step << 6)};
      auto const candidate{std::bit_cast<int16_t>(static_cast<uint16_t>(old < speed ? old + amount : old - amount))};
      advance_speed_motion(pose, static_cast<uint16_t>(old < speed ? std::min(candidate, speed) : std::max(candidate, speed)), frame_step);
      return;
    }
    hangar.deadline = static_cast<uint16_t>(clock + settling_duration_ticks);
    hangar.returning = hangar_return_phase::settling;
  }
  if(hangar.returning != hangar_return_phase::settling) return;
  auto const remaining{static_cast<uint16_t>(hangar.deadline - clock)};
  if(remaining & 0x8000) {
    hangar.returning = hangar_return_phase::complete;
    return;
  }
  auto const target_heading{static_cast<uint16_t>(player.tunnel ? 0 : 0x8000)};
  auto const heading{std::bit_cast<int16_t>(static_cast<uint16_t>(pose.angles.heading - target_heading))};
  parameters.bank_limit = static_cast<uint16_t>(heading ^ (heading < 0 ? -1 : 0));
  steer(player.tunnel ? 0x0a20 : 0, static_cast<uint16_t>(target_heading - pose.angles.heading));
  hangar.extension = remaining < door_closing_delay_ticks ? 0 : static_cast<uint16_t>(std::min<unsigned int>((remaining - door_closing_delay_ticks) << 6, maximum_door_extension));
}

} // namespace darker::game
