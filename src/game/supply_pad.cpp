#include "game/supply_pad.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include "game/actor_motion.h"
#include "game/city_map.h"
#include "game/object_definitions.h"
#include "game/player_flight.h"
#include "maths/direction.h"
#include "maths/sine_table.h"

namespace darker::game {
namespace {

int constexpr pad_surface_height{264};

uint16_t approach_word(uint16_t const value, uint16_t const target, uint16_t const step) noexcept {
  /// 7CDB compares signed words before and after a wrapping add or subtract
  auto const current{std::bit_cast<int16_t>(value)}, desired{std::bit_cast<int16_t>(target)};
  bool const below{current < desired};
  auto const next{std::bit_cast<int16_t>(static_cast<uint16_t>(below ? value+step : value-step))};
  return static_cast<uint16_t>(below ? std::min(next,desired) : std::max(next,desired));
}

void approach_position(object_pose &pose, maths::map_position const target, uint16_t const step) noexcept {
  /// 7CA4 reduces the major horizontal error and scales the minor error by the remaining ratio
  std::array<int,2> error{std::bit_cast<int16_t>(static_cast<uint16_t>(pose.position.column-target.column)),
    std::bit_cast<int16_t>(static_cast<uint16_t>(pose.position.row-target.row))};
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
  if((site & 1) || (site >> 8) >= city_map_size.row) throw std::invalid_argument{"Invalid Skimma starting site"};
  auto const *previous{std::get_if<skimma_flight_state>(&player.craft)};
  uint16_t constexpr restored_shield_charge{0xbf00};
  auto const shield{static_cast<uint16_t>(restored_shield_charge | (previous ? previous->damage.shield_charge & 255 : 0))};
  player = {};
  player.craft = skimma_flight_state{
    .pose{
      .position{
        .column{static_cast<uint16_t>(((site & 255) >> 1)*256+128)},
        .row{static_cast<uint16_t>((site & 0xff00)+128)},
        .height{static_cast<uint16_t>(pad_surface_height-model_height)}
      },
      .angles{
        .heading{static_cast<uint16_t>(heading*256)},
        .pitch{0},
        .roll{0}
      }
    },
    .damage{
      .shield_charge{shield}
    }
  };
  player.engine_flags = 0;
  player.forward_setting = 256;
  player.lifecycle.flags = 0x10;
  player.upgraded = upgraded;
  player.supply = {
    .phase{upgraded ? supply_phase::docked : supply_phase::flight},
    .site{site}
  };
}

bool begin_supply_approach(player_flight &player, city_map const &cells, supply_pad_state &pad) noexcept {
  /// C6FB accepts a descending, unshielded Skimma over the heading-projected inner area of a type-three pad
  int constexpr maximum_landing_speed{320};
  int constexpr capture_height_range{760};
  int constexpr edge_exclusion_width{36};
  int constexpr edge_exclusion_diameter{2 * edge_exclusion_width};
  int constexpr supply_pad_model_type{3};
  auto const *craft{std::get_if<skimma_flight_state>(&player.craft)};
  if(!craft || (player.lifecycle.flags & 0x10) || (player.engine_flags & 1)) return false;
  if((craft->vertical_velocity >> 8) != 255 || craft->horizontal_velocity >= maximum_landing_speed) return false;
  auto const &pose{craft->pose};
  if(static_cast<uint16_t>(pose.position.height-pad_surface_height) >= capture_height_range) return false;
  auto const heading{pose.angles.heading >> 6};
  auto const x{static_cast<uint16_t>(pose.position.column-(maths::original_sine[heading] >> 8))};
  auto const y{static_cast<uint16_t>(pose.position.row-(maths::original_sine[(heading+256)%1024] >> 8))};
  if((x | y) & 0x8000) return false;
  if(static_cast<uint8_t>(x+edge_exclusion_width) < edge_exclusion_diameter || static_cast<uint8_t>(y+edge_exclusion_width) < edge_exclusion_diameter) return false;
  if(cells[(y >> 8) * city_map_size.column+(x >> 8)].type != supply_pad_model_type) return false;
  pad.phase = supply_phase::approach;
  pad.site = static_cast<uint16_t>((y & 0xff00) | ((x >> 8)*2));
  pad.offset = static_cast<uint16_t>((y << 8) | (x & 255));
  player.lifecycle.flags |= 0x10;
  return true;
}

void advance_supply_motion(player_flight &player, supply_pad_state &pad, uint16_t const output,
  bool const supplementary_active, uint16_t const pitch_control, uint16_t const frame_step) {
  /// 7D9A centres in two stages; the shared 7E49 tail ramps output and admits a deliberate pitch-controlled departure
  int constexpr docking_height{328};
  uint16_t constexpr centred_pad_offset{0x8080};
  int constexpr docking_turn_rate{7};
  int constexpr minimum_departure_pitch_input{12};
  int constexpr departure_vertical_velocity{200};
  if(pad.phase == supply_phase::flight || !frame_step) return;
  auto &craft{std::get<skimma_flight_state>(player.craft)};
  auto &pose{craft.pose};
  if(pad.phase == supply_phase::approach) {
    auto const &definition{original_object_definitions[player.upgraded ? 27 : 26]};
    actor_attitude attitude{craft.damage.rotation.pitch,craft.damage.rotation.turn};
    steer_actor(pose,attitude,{
      .response{static_cast<uint16_t>(definition.angular_seed*8)},
      .bank_response{static_cast<uint16_t>(definition.motion_seeds.bank_response*256)},
      .bank_limit{static_cast<uint16_t>(definition.motion_seeds.bank_limit*64)},
      .turn_response{static_cast<uint16_t>(definition.motion_seeds.turn_response*256)}
    },0,0,frame_step);
    craft.damage.rotation = {attitude.pitch_rate,attitude.bank_rate};
    maths::map_position const target{
      .column{static_cast<uint16_t>(((pad.site & 255) >> 1)*256+(pad.offset & 255))},
      .row{static_cast<uint16_t>((pad.site & 0xff00)+(pad.offset >> 8))}
    };
    auto const fractional{static_cast<unsigned int>(pad.fraction)+static_cast<uint8_t>(frame_step << 5)};
    auto const step{static_cast<uint16_t>((frame_step >> 3)+(fractional >> 8))};
    pad.fraction = static_cast<uint8_t>(fractional);
    approach_position(pose,target,step);
    pose.position.height = approach_word(pose.position.height,docking_height,step);
    if(pose.position.column == target.column && pose.position.row == target.row && pose.position.height == docking_height) {
      auto const dx{static_cast<uint16_t>(128-(pose.position.column & 255))};
      auto const dy{static_cast<uint16_t>(128-(pose.position.row & 255))};
      auto const desired{static_cast<uint16_t>((dx | dy) ? (maths::direction_index(dx,dy) << 5)^0x8000 : 0)};
      auto const error{std::bit_cast<int16_t>(static_cast<uint16_t>(desired-pose.angles.heading))};
      auto const amount{std::min(error < 0 ? -static_cast<int>(error) : static_cast<int>(error),static_cast<int>(static_cast<uint16_t>(frame_step*docking_turn_rate)))};
      pose.angles.heading = static_cast<uint16_t>(pose.angles.heading+(error < 0 ? -amount : amount));
      if(!amount) {
        if(pad.offset == centred_pad_offset) pad.phase = supply_phase::docked;
        pad.offset = centred_pad_offset;
      }
    }
  }
  craft.horizontal_velocity = approach_word(craft.horizontal_velocity,output,frame_step);
  pose.speed = craft.horizontal_velocity >> 1;
  if(!(pose.speed >> 8) || supplementary_active || std::bit_cast<int8_t>(static_cast<uint8_t>(pitch_control >> 8)) < minimum_departure_pitch_input) return;
  craft.horizontal_velocity >>= 1;
  craft.vertical_velocity = departure_vertical_velocity;
  player.lifecycle.flags &= 0xef;
  pad.phase = supply_phase::flight;
}

} // namespace darker::game
