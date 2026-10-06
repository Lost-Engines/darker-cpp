#include "game/hangar.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include "game/actor_motion.h"
#include "game/flight_motion.h"
#include "game/object_definitions.h"
#include "maths/direction.h"

namespace darker::game {
namespace {

std::size_t site_index(std::uint16_t const site) {
  /// Return sites use the world's packed byte address, with twice the column in its low byte
  auto const column{(site & 255) >> 1};
  auto const row{site >> 8};
  if((site & 1) || row == 0 || row >= 127) throw std::invalid_argument{"Invalid Caero hangar site"};
  return row * 128 + column;
}

void toggle_hangar(player_flight &player, city_map &cells, std::size_t const centre) {
  /// C6D2 toggles the landed flag and the gate, approach-light and interior alternate states together
  player.lifecycle.flags ^= 0x10;
  for(auto const index : {centre - 128, centre, centre + 128}) cells[index].state ^= 0x80;
}

} // namespace

void initialise_caero_hangar(player_flight &player, city_map &cells, hangar_state &hangar, std::int16_t const model_height) {
  /// BD34–BD98 places the Caero at the type-17 return sites used by the Delphi campaign
  auto const centre{site_index(hangar.return_site)};
  if(cells[centre].type != 17) throw std::invalid_argument{"Caero launch requires a campaign type-17 hangar"};
  player = {};
  // Outer startup 3D50–3D52 supplies thrust energy independently of the visible boost-cell reserve.
  std::get<caero_flight_state>(player.craft).energy.buffer = 0x6000;
  player.pose() = {
    .position{static_cast<std::uint16_t>((centre % 128) * 256 + 128), static_cast<std::uint16_t>((centre / 128) * 256 + 152),
      static_cast<std::uint16_t>(-104 - model_height)},
    .angles{0, 0x0a20, 0},
  };
  hangar.extension = 0;
  hangar.sound_level = 0;
  toggle_hangar(player, cells, centre);
}

void advance_hangar_departure(player_flight &player, city_map &cells, hangar_state &hangar, std::uint16_t const frame_step) {
  /// C5F9 opens the gate inside its cell, retracts it over the approach lights and closes the site after departure
  if(!(player.lifecycle.flags & 0x10) || player.lifecycle.crashing) return;
  if(hangar.returning == hangar_return_phase::settling || hangar.returning == hangar_return_phase::complete) return;
  auto const centre{site_index(hangar.return_site)};
  auto const &position{player.pose().position};
  auto const column{static_cast<unsigned int>(position[0] >> 8)};
  auto const row{static_cast<unsigned int>(position[1] >> 8)};
  auto const type{column < 128 && row < 128 ? cells[row * 128 + column].type : 255};
  if(type < 17 || type > 24) {
    if(hangar.returning == hangar_return_phase::approaching) return;
    hangar.extension = 0;
    hangar.sound_level = 0;
    toggle_hangar(player, cells, centre);
    return;
  }
  if(type & 1) {
    auto const delta{static_cast<std::uint16_t>(frame_step << 6)};
    auto const sum{static_cast<unsigned int>(hangar.extension) + delta};
    if(sum >= 0xe800) {
      hangar.extension = 0xe800;
      return;
    }
    hangar.extension = static_cast<std::uint16_t>(sum);
  } else {
    auto const distance{horizontal_distance(position, {static_cast<std::uint16_t>((centre % 128) * 256 + 128),
      static_cast<std::uint16_t>((centre / 128) * 256 + 152), 0})};
    auto value{static_cast<std::uint16_t>(152 - distance)};
    if(distance != 0 && distance <= 152) value = 65535;
    unsigned int const sum{value + 232u};
    hangar.extension = sum > 65535 ? static_cast<std::uint16_t>((sum & 255) * 257) : 0;
  }
  hangar.sound_level = static_cast<std::uint8_t>((hangar.extension & 255) | (hangar.extension >> 8));
}

bool begin_hangar_return(player_flight &player, city_map &cells, hangar_state &hangar, bool const objectives_complete) {
  /// C670 admits the type-17 approach only after objectives clear and position/attitude fit the original capture window
  if(!objectives_complete || player.lifecycle.flags & 0x30 || hangar.returning != hangar_return_phase::none) return false;
  auto const centre{site_index(hangar.return_site)};
  if(cells[centre].type != 17) throw std::invalid_argument{"Return capture currently requires a type-17 hangar"};
  auto const &pose{player.pose()};
  if((pose.position[2] >> 8) >= 9) return false;
  auto const aligned{[](uint16_t const error){ return static_cast<uint8_t>((error >> 8) + 7) < 14; }};
  if(!aligned(pose.angles[2])) return false;
  std::array<uint16_t, 3> const target{static_cast<uint16_t>((centre % 128) * 256 + 128),
    static_cast<uint16_t>((centre / 128) * 256 + 152), 256};
  auto const distance{horizontal_distance(pose.position, target)};
  if(static_cast<uint16_t>(distance - 0x260) >= 256) return false;
  auto const direction{maths::object_target_direction(pose.position, target)};
  if(!aligned(static_cast<uint16_t>(direction.pitch - pose.angles[1]))
    || !aligned(static_cast<uint16_t>(direction.heading - pose.angles[0]))
    || !aligned(static_cast<uint16_t>(0x8000 - direction.heading))) return false;
  hangar.returning = hangar_return_phase::approaching;
  toggle_hangar(player, cells, centre);
  return true;
}

void advance_hangar_return(player_flight &player, hangar_state &hangar, uint16_t frame_step, uint16_t const clock) {
  /// 7CEF/7D32 steer through the approach and inner berth, then turn in place until the original completion deadline
  auto &craft{std::get<caero_flight_state>(player.craft)};
  auto &pose{craft.pose};
  auto const &definition{original_object_definitions[25]};
  actor_attitude attitude{craft.damage.rotation.pitch, craft.damage.rotation.turn};
  actor_steering_parameters parameters{
    .response{static_cast<uint16_t>(definition.angular_seed * 8)},
    .bank_response{static_cast<uint16_t>(definition.motion_seeds[0] * 256)},
    .bank_limit{static_cast<uint16_t>(definition.motion_seeds[1] * 64)},
    .turn_response{static_cast<uint16_t>(definition.motion_seeds[2] * 256)},
  };
  auto const steer{[&](uint16_t const pitch, uint16_t const drive){
    frame_step = steer_actor(pose, attitude, parameters, pitch, drive, frame_step);
    craft.damage.rotation = {attitude.pitch_rate, attitude.bank_rate};
  }};
  if(hangar.returning == hangar_return_phase::approaching) {
    auto const centre{site_index(hangar.return_site)};
    std::array<uint16_t, 3> target{static_cast<uint16_t>((centre % 128) * 256 + 128),
      static_cast<uint16_t>((centre / 128) * 256 + 216), 220};
    auto const distance{horizontal_distance(pose.position, target)};
    auto const approach{[&](std::array<uint16_t, 3> const &point){
      bool const close{static_cast<uint16_t>(point[0] - pose.position[0] + 7) < 15
        && static_cast<uint16_t>(point[1] - pose.position[1] + 7) < 15};
      if(close) return true;
      auto const direction{maths::object_target_direction(pose.position, point)};
      auto const difference{static_cast<uint16_t>(direction.heading - pose.angles[0])};
      if(((difference ^ static_cast<uint16_t>(difference << 1)) & 0x8000) != 0) return true;
      steer(direction.pitch, difference);
      return false;
    }};
    auto outer{target};
    outer[1] -= 256;
    bool arrived{false};
    if(approach(outer)) {
      target[2] = static_cast<uint16_t>(std::min<int>(static_cast<int>(distance >> 2) - 40, 41));
      arrived = approach(target);
    }
    if(!arrived) {
      auto const speed{static_cast<int16_t>(std::min<unsigned int>(static_cast<uint16_t>(distance + 80), 280))};
      auto const old{std::bit_cast<int16_t>(pose.speed)};
      auto const amount{static_cast<uint16_t>(frame_step << 6)};
      auto const candidate{std::bit_cast<int16_t>(static_cast<uint16_t>(old < speed ? old + amount : old - amount))};
      advance_speed_motion(pose, static_cast<uint16_t>(old < speed ? std::min(candidate, speed) : std::max(candidate, speed)), frame_step);
      return;
    }
    hangar.deadline = static_cast<uint16_t>(clock + 0x7ff);
    hangar.returning = hangar_return_phase::settling;
  }
  if(hangar.returning != hangar_return_phase::settling) return;
  auto const remaining{static_cast<uint16_t>(hangar.deadline - clock)};
  if(remaining & 0x8000) { hangar.returning = hangar_return_phase::complete; return; }
  auto const heading{std::bit_cast<int16_t>(static_cast<uint16_t>(pose.angles[0] - 0x8000))};
  parameters.bank_limit = static_cast<uint16_t>(heading ^ (heading < 0 ? -1 : 0));
  steer(0, static_cast<uint16_t>(0x8000 - pose.angles[0]));
  hangar.extension = remaining < 1024 ? 0 : static_cast<uint16_t>(std::min<unsigned int>((remaining - 1024) << 6, 0xe800));
}

} // namespace darker::game
