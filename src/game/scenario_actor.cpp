#include "game/scenario_actor.h"
#include <algorithm>
#include <stdexcept>
#include "game/object_definitions.h"

namespace darker::game {

scenario_actor make_scenario_actor(resources::scenario_placement const &placement,
  object_definition const &definition, std::uint16_t const model_token, std::int16_t const model_height,
  std::uint8_t const index, std::uint8_t const world_mode, std::size_t const shared_offset) {
  /// BE07/BF1A constructs a cleared scenario record; category lists and embedded setup are owned by the world
  if(world_mode > 2) throw std::invalid_argument{"Unknown scenario actor world mode"};
  bool const moving{placement.form == resources::placement_form::moving};
  if(moving && world_mode == 2) throw std::invalid_argument{"Underground actor creation requires route initialisation"};
  scenario_actor actor{
    .category{placement.form == resources::placement_form::absolute_static ? actor_category::stationary
      : placement.definition_slot > 28 ? actor_category::ground : actor_category::air},
    .index{index},
    .definition_slot{placement.definition_slot},
    .attributes{static_cast<std::uint8_t>((moving ? placement.attributes * 2 : 0) | static_cast<unsigned int>(placement.counted))},
  };
  apply_object_definition(actor.parameters, definition, model_token);
  int const height{moving ? definition.role_data[2] * 256 : world_mode == 2 && placement.form == resources::placement_form::absolute_static ? 128 : 0};
  actor.pose.position = {placement.position[0], placement.position[1], static_cast<std::uint16_t>(height - model_height)};
  actor.pose.angles[0] = placement.heading;
  actor.previous_position = actor.pose.position;
  actor.current_cell = static_cast<std::uint16_t>((placement.position[0] >> 8) | (placement.position[1] & 0xff00));
  actor.target_token = actor.current_cell;
  if(moving) {
    actor.parameters.update_entry = 0x8823;
    actor.pose.speed = static_cast<std::uint16_t>(definition.base_speed * 16);
    actor.flags = 2;
    actor.behaviour = placement.motion;
  } else if(placement.form == resources::placement_form::compact_special) {
    actor.parameters.update_entry = 0x8f3b;
    actor.route.emplace();
  } else {
    actor.parameters.update_entry = 0;
    return actor;
  }
  if(placement.program_offset) {
    if(*placement.program_offset < shared_offset) throw std::invalid_argument{"Scenario actor program precedes shared section"};
    auto const cursor{*placement.program_offset - shared_offset};
    if(actor.route) actor.route->cursor = cursor;
    else {
      actor.script.continuation = cursor;
      actor.script.checkpoint = cursor;
    }
  } else {
    if(!placement.script_or_target) throw std::invalid_argument{"Scenario actor has no script or target"};
    auto const target{static_cast<std::uint8_t>(*placement.script_or_target)};
    if(target != 255) actor.target_token = static_cast<std::uint16_t>(0xd986 + target * 112);
    actor.script.stopped = true;
  }
  return actor;
}

std::vector<scenario_actor> make_scenario_group(resources::scenario_group const &group, resources::geometry_bank const &bank,
  std::uint8_t first_index, std::uint8_t const world_mode, std::size_t const shared_offset) {
  /// Construct source records with stable native indices, then preserve head insertion order within a category
  if(!group.native_setup.empty()) throw std::invalid_argument{"Scenario group requires embedded setup operations"};
  if(group.objects.size() > 255u - first_index) throw std::invalid_argument{"Scenario group exceeds the object index range"};
  std::vector<scenario_actor> actors;
  actors.reserve(group.objects.size());
  for(auto const &placement : group.objects) {
    if(placement.definition_slot >= original_object_definitions.size() || placement.definition_slot >= bank.special_models().size()) {
      throw std::invalid_argument{"Scenario object definition is outside the loaded bank"};
    }
    auto const model{bank.special_models()[placement.definition_slot]};
    actors.push_back(make_scenario_actor(placement, original_object_definitions[placement.definition_slot],
      model, bank.header_at(model).height, first_index++, world_mode, shared_offset));
  }
  std::ranges::reverse(actors);
  return actors;
}

} // namespace darker::game
