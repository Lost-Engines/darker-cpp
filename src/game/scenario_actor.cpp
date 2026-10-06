#include "game/scenario_actor.h"
#include <stdexcept>

namespace darker::game {

scenario_actor make_scenario_actor(resources::scenario_placement const &placement,
  object_definition const &definition, std::uint16_t const model_token, std::int16_t const model_height,
  std::uint8_t const index, std::uint8_t const world_mode, std::size_t const shared_offset) {
  /// BE07/BF1A constructs a cleared scenario record; category lists and embedded setup are owned by the world
  if(world_mode > 2) throw std::invalid_argument{"Unknown scenario actor world mode"};
  bool const moving{placement.form == resources::placement_form::moving};
  if(moving && world_mode == 2) throw std::invalid_argument{"Underground actor creation requires route initialisation"};
  scenario_actor actor{
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
    actor.script.deadline = 0xf000;
  } else {
    actor.parameters.update_entry = 0;
    return actor;
  }
  if(placement.program_offset) {
    if(*placement.program_offset < shared_offset) throw std::invalid_argument{"Scenario actor program precedes shared section"};
    actor.script.continuation = *placement.program_offset - shared_offset;
    actor.script.checkpoint = actor.script.continuation;
  } else {
    if(!placement.script_or_target) throw std::invalid_argument{"Scenario actor has no script or target"};
    auto const target{static_cast<std::uint8_t>(*placement.script_or_target)};
    if(target != 255) actor.target_token = static_cast<std::uint16_t>(0xd986 + target * 112);
    actor.script.stopped = true;
  }
  return actor;
}

} // namespace darker::game
