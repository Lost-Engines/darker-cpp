#include "game/supply_pad.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include "game/actor_motion.h"
#include "game/object_definitions.h"
#include "game/player_flight.h"
#include "maths/direction.h"
#include "maths/sine_table.h"

namespace darker::game {
namespace {

uint16_t approach_word(uint16_t const value, uint16_t const target, uint16_t const step) noexcept {
  /// 7CDB compares signed words before and after a wrapping add or subtract
  auto const current{std::bit_cast<int16_t>(value)}, desired{std::bit_cast<int16_t>(target)};
  bool const below{current < desired};
  auto const next{std::bit_cast<int16_t>(static_cast<uint16_t>(below ? value+step : value-step))};
  return static_cast<uint16_t>(below ? std::min(next,desired) : std::max(next,desired));
}

void approach_position(object_pose &pose, std::array<uint16_t,2> const target, uint16_t const step) noexcept {
  /// 7CA4 reduces the major horizontal error and scales the minor error by the remaining ratio
  std::array<int,2> error{std::bit_cast<int16_t>(static_cast<uint16_t>(pose.position[0]-target[0])),
    std::bit_cast<int16_t>(static_cast<uint16_t>(pose.position[1]-target[1]))};
  auto const magnitude{[](int const value){ return value < 0 ? -value : value; }};
  auto const largest{std::max(magnitude(error[0]),magnitude(error[1]))};
  auto const remaining{std::max(largest-static_cast<int>(step),0)};
  auto const denominator{remaining+step};
  for(size_t i{0}; i < 2; ++i) {
    auto const distance{denominator ? remaining*magnitude(error[i])/denominator : 0};
    pose.position[i] = static_cast<uint16_t>(target[i]+(error[i] < 0 ? -distance : distance));
  }
}

} // namespace

void initialise_skimma_pad(player_flight &player, uint16_t const site, uint8_t const heading,
  int16_t const model_height, bool const upgraded) {
  /// BD34's mode-one path starts centred at nominal height 264; C7B3 restores only the shield resource's high byte
  if((site & 1) || (site >> 8) >= 128) throw std::invalid_argument{"Invalid Skimma starting site"};
  auto const *previous{std::get_if<skimma_flight_state>(&player.craft)};
  auto const shield{static_cast<uint16_t>(0xbf00 | (previous ? previous->damage.shield_charge & 255 : 0))};
  player = {};
  player.craft = skimma_flight_state{.pose{
    .position{static_cast<uint16_t>(((site & 255) >> 1)*256+128),static_cast<uint16_t>((site & 0xff00)+128),static_cast<uint16_t>(264-model_height)},
    .angles{static_cast<uint16_t>(heading*256),0,0}},.damage{.shield_charge{shield}}};
  player.engine_flags = 0;
  player.forward_setting = 256;
  player.lifecycle.flags = 0x10;
  player.upgraded = upgraded;
  player.supply = {.phase{upgraded ? supply_phase::docked : supply_phase::flight},.site{site}};
}

bool begin_supply_approach(player_flight &player, city_map const &cells, supply_pad_state &pad) noexcept {
  /// C6FB accepts a descending, unshielded Skimma over the heading-projected inner area of a type-three pad
  auto const *craft{std::get_if<skimma_flight_state>(&player.craft)};
  if(!craft || (player.lifecycle.flags & 0x10) || (player.engine_flags & 1)) return false;
  if((craft->vertical_velocity >> 8) != 255 || craft->horizontal_velocity >= 320) return false;
  auto const &pose{craft->pose};
  if(static_cast<uint16_t>(pose.position[2]-264) >= 760) return false;
  auto const heading{pose.angles[0] >> 6};
  auto const x{static_cast<uint16_t>(pose.position[0]-(maths::original_sine[heading] >> 8))};
  auto const y{static_cast<uint16_t>(pose.position[1]-(maths::original_sine[(heading+256)%1024] >> 8))};
  if((x | y) & 0x8000) return false;
  if(static_cast<uint8_t>(x+36) < 72 || static_cast<uint8_t>(y+36) < 72) return false;
  if(cells[(y >> 8)*128+(x >> 8)].type != 3) return false;
  pad.phase = supply_phase::approach;
  pad.site = static_cast<uint16_t>((y & 0xff00) | ((x >> 8)*2));
  pad.offset = static_cast<uint16_t>((y << 8) | (x & 255));
  player.lifecycle.flags |= 0x10;
  return true;
}

void advance_supply_motion(player_flight &player, supply_pad_state &pad, uint16_t const output,
  bool const supplementary_active, uint16_t const pitch_control, uint16_t const frame_step) {
  /// 7D9A centres in two stages; the shared 7E49 tail ramps output and admits a deliberate pitch-controlled departure
  if(pad.phase == supply_phase::flight || !frame_step) return;
  auto &craft{std::get<skimma_flight_state>(player.craft)};
  auto &pose{craft.pose};
  if(pad.phase == supply_phase::approach) {
    auto const &definition{original_object_definitions[player.upgraded ? 27 : 26]};
    actor_attitude attitude{craft.damage.rotation.pitch,craft.damage.rotation.turn};
    steer_actor(pose,attitude,{.response{static_cast<uint16_t>(definition.angular_seed*8)},
      .bank_response{static_cast<uint16_t>(definition.motion_seeds[0]*256)},
      .bank_limit{static_cast<uint16_t>(definition.motion_seeds[1]*64)},
      .turn_response{static_cast<uint16_t>(definition.motion_seeds[2]*256)}},0,0,frame_step);
    craft.damage.rotation = {attitude.pitch_rate,attitude.bank_rate};
    std::array<uint16_t,2> const target{static_cast<uint16_t>(((pad.site & 255) >> 1)*256+(pad.offset & 255)),
      static_cast<uint16_t>((pad.site & 0xff00)+(pad.offset >> 8))};
    auto const fractional{static_cast<unsigned int>(pad.fraction)+static_cast<uint8_t>(frame_step << 5)};
    auto const step{static_cast<uint16_t>((frame_step >> 3)+(fractional >> 8))};
    pad.fraction = static_cast<uint8_t>(fractional);
    approach_position(pose,target,step);
    pose.position[2] = approach_word(pose.position[2],328,step);
    if(pose.position[0] == target[0] && pose.position[1] == target[1] && pose.position[2] == 328) {
      auto const dx{static_cast<uint16_t>(128-(pose.position[0] & 255))};
      auto const dy{static_cast<uint16_t>(128-(pose.position[1] & 255))};
      auto const desired{static_cast<uint16_t>((dx | dy) ? (maths::direction_index(dx,dy) << 5)^0x8000 : 0)};
      auto const error{std::bit_cast<int16_t>(static_cast<uint16_t>(desired-pose.angles[0]))};
      auto const amount{std::min(error < 0 ? -static_cast<int>(error) : static_cast<int>(error),static_cast<int>(static_cast<uint16_t>(frame_step*7)))};
      pose.angles[0] = static_cast<uint16_t>(pose.angles[0]+(error < 0 ? -amount : amount));
      if(!amount) {
        if(pad.offset == 0x8080) pad.phase = supply_phase::docked;
        pad.offset = 0x8080;
      }
    }
  }
  craft.horizontal_velocity = approach_word(craft.horizontal_velocity,output,frame_step);
  pose.speed = craft.horizontal_velocity >> 1;
  if(!(pose.speed >> 8) || supplementary_active || std::bit_cast<int8_t>(static_cast<uint8_t>(pitch_control >> 8)) < 12) return;
  craft.horizontal_velocity >>= 1;
  craft.vertical_velocity = 200;
  player.lifecycle.flags &= 0xef;
  pad.phase = supply_phase::flight;
}

} // namespace darker::game
