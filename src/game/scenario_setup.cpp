#include "game/scenario_setup.h"
#include <algorithm>
#include <stdexcept>

namespace darker::game {

scenario_setup_kind identify_scenario_setup(std::span<std::byte const> const code) {
  /// Recognise the seven audited inline programs without executing resource-supplied machine code
  static constexpr std::array<uint8_t,30> block_0{0x83,0xc6,0x1e,0xb8,0x0,0xff,0xbf,0x0,0xec,0xc7,0x46,0x3e,0xc8,0x0,0xc7,0x46,0x42,0x90,0x1,0xb9,0x0,0xa,0x80,0x66,0x7,0xef,0x2b,0xd2,0xff,0x27};
  static constexpr std::array<uint8_t,40> block_1{0x83,0xc6,0x28,0xb9,0x8,0x1,0xc6,0x46,0x8,0xc8,0xc6,0x46,0xa,0xbb,0x8a,0x46,0x9,0x88,0x44,0x3,0x88,0x44,0x11,0x8a,0x46,0xb,0x88,0x44,0x5,0x88,0x44,0x12,0x2b,0xc0,0xbf,0x20,0xf,0x99,0xff,0x27};
  static constexpr std::array<uint8_t,7> block_2{0x83,0xc6,0x7,0xfe,0x46,0xd,0xc3};
  static constexpr std::array<uint8_t,27> block_3{0x83,0xc6,0x1b,0xc6,0x46,0x8,0x4b,0xc6,0x46,0xa,0x46,0x81,0x6e,0xc,0x60,0x9,0x8b,0xfb,0xbb,0xc8,0x0,0xb8,0x0,0xc,0xff,0x65,0x2};
  static constexpr std::array<uint8_t,35> block_4{0x83,0xc6,0x23,0xb8,0x0,0xfe,0x8b,0x7e,0x2a,0xc7,0x46,0x3e,0x2c,0x1,0xc7,0x46,0x42,0x58,0x2,0xb9,0x0,0x10,0x80,0x66,0x7,0xef,0x2b,0xd2,0x2a,0xc0,0xff,0x57,0x4,0xff,0x27};
  static constexpr std::array<uint8_t,28> block_5{0x83,0xc6,0x1c,0x8b,0x46,0x22,0x89,0x44,0x22,0xb8,0x0,0xf5,0x8b,0x7e,0x2a,0xb9,0x80,0x9,0x80,0x66,0x7,0xef,0xba,0xff,0x9f,0xf9,0xff,0x27};
  static constexpr std::array<uint8_t,9> block_6{0x83,0xc6,0x9,0xc7,0x46,0x22,0x0,0x0,0xc3};
  std::array<std::span<uint8_t const>,7> const blocks{block_0,block_1,block_2,block_3,block_4,block_5,block_6};
  for(size_t i{0}; i < blocks.size(); ++i) {
    if(std::ranges::equal(code,blocks[i],[](std::byte const a, uint8_t const b){ return std::to_integer<uint8_t>(a) == b; }))
      return static_cast<scenario_setup_kind>(i);
  }
  throw std::invalid_argument{"Unrecognised embedded scenario setup"};
}

void apply_player_scenario_setup(scenario_setup_kind const kind, player_flight &player,
  int16_t const model_height, weapon_ammunition &second_weapon) {
  /// C79D sets energy and boost before C7C8 applies the model-dependent height and approach angles
  auto &pose{player.pose()};
  uint16_t height{0}, energy{0};
  bool boost{false};
  switch(kind) {
  case scenario_setup_kind::halon_approach:
    pose.speed = 200;
    std::visit([](auto &craft){ craft.horizontal_velocity = 400; },player.craft);
    player.lifecycle.flags &= 0xef;
    pose.angles.pitch = 0xff00;
    pose.angles.heading = 0xec00;
    height = 2560;
    break;
  case scenario_setup_kind::anchor_escorts:
    pose.position[0] = static_cast<uint16_t>((pose.position[0] & 0xff00) | 0xc8);
    pose.position[1] = static_cast<uint16_t>((pose.position[1] & 0xff00) | 0xbb);
    pose.angles.pitch = 0;
    pose.angles.heading = 0x0f20;
    height = 264;
    break;
  case scenario_setup_kind::final_approach:
    pose.speed = 300;
    std::visit([](auto &craft){ craft.horizontal_velocity = 600; },player.craft);
    player.lifecycle.flags &= 0xef;
    pose.angles.pitch = 0xfe00;
    // 5E59 clears 5E11/5E14: the zero-based slot one, displayed as weapon 2.
    second_weapon = {};
    height = 4096;
    break;
  case scenario_setup_kind::nightmare_player:
    player.lifecycle.flags &= 0xef;
    pose.angles.pitch = 0xf500;
    height = 2432;
    energy = 0x9fff;
    boost = true;
    break;
  default: throw std::invalid_argument{"Actor setup cannot be applied to the player"};
  }
  pose.position[2] = static_cast<uint16_t>(height-model_height);
  if(auto *caero{std::get_if<caero_flight_state>(&player.craft)}) {
    caero->energy.boost = energy;
    caero->energy.reserve = energy;
    caero->active_boost = static_cast<uint16_t>((caero->active_boost & 255) | (boost ? 0x2800 : 0));
  }
}

void apply_actor_scenario_setup(scenario_setup_kind const kind, scenario_actor &actor,
  uint16_t const player_model, uint16_t const clock) {
  /// Apply mutations to the most recently allocated actor, retaining fields the native block leaves alone
  switch(kind) {
  case scenario_setup_kind::raise_actor:
    actor.pose.position[2] = static_cast<uint16_t>(actor.pose.position[2]+256);
    break;
  case scenario_setup_kind::escort_departure:
    actor.pose.position[0] = static_cast<uint16_t>((actor.pose.position[0] & 0xff00) | 0x4b);
    actor.pose.position[1] = static_cast<uint16_t>((actor.pose.position[1] & 0xff00) | 0x46);
    actor.pose.position[2] = static_cast<uint16_t>(actor.pose.position[2]-2400);
    actor.pose.speed = 200;
    actor.pose.angles.pitch = 0x0c00;
    actor.expiry = static_cast<uint16_t>(clock+256);
    actor.script.deadline = static_cast<uint16_t>(clock+1024);
    actor.parameters.update_entry = object_update::departing_aircraft;
    break;
  case scenario_setup_kind::copy_player_model:
    actor.parameters.model_token = player_model;
    break;
  default: throw std::invalid_argument{"Player setup cannot be applied to an actor"};
  }
}

std::array<std::vector<scenario_actor>,3> make_scenario_actors(resources::scenario_record const &record,
  resources::scenario_resource const &resource, resources::geometry_bank const &bank, player_flight &player,
  weapon_ammunition &second_weapon, uint16_t const clock, std::optional<tunnel_setup> const tunnel) {
  /// Preserve allocation order and translate the inline player, placement and actor mutations between groups
  auto const configuration{record.configuration & 15};
  uint8_t const world{static_cast<uint8_t>(configuration == 4 ? 2 : configuration <= 1 ? 0 : 1)};
  auto const player_model{bank.special_models()[configuration == 4 ? 28 : 24+configuration]};
  std::array<std::vector<scenario_actor>,3> result;
  uint8_t first{1};
  for(size_t i{0}; i < result.size(); ++i) {
    auto group{record.groups[i]};
    for(auto const &block : group.native_setup) {
      if(block.current_object != 0) continue;
      auto const kind{identify_scenario_setup(resource.bytes(block.source))};
      apply_player_scenario_setup(kind,player,bank.header_at(player_model).height,second_weapon);
      if(kind == scenario_setup_kind::anchor_escorts) {
        if(group.objects.size() < 2) throw std::invalid_argument{"Escort setup requires two following actors"};
        for(size_t j{0}; j < 2; ++j) for(size_t axis{0}; axis < 2; ++axis)
          group.objects[j].position[axis] = static_cast<uint16_t>((group.objects[j].position[axis] & 255) | (player.pose().position[axis] & 0xff00));
      }
    }
    group.native_setup.clear();
    result[i] = make_scenario_group(group,bank,first,world,record.shared.offset,tunnel);
    first = static_cast<uint8_t>(first+group.objects.size());
    for(auto const &block : record.groups[i].native_setup) {
      if(block.current_object == 0) continue;
      auto const found{std::ranges::find(result[i],block.current_object,&scenario_actor::index)};
      if(found == result[i].end()) throw std::invalid_argument{"Embedded setup has no current actor"};
      apply_actor_scenario_setup(identify_scenario_setup(resource.bytes(block.source)),*found,player_model,clock);
    }
  }
  return result;
}

} // namespace darker::game
