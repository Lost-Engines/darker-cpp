#include "game/actor_update.h"
#include <algorithm>
#include <stdexcept>

namespace darker::game {

std::optional<uint8_t> advance_surface_actor(scenario_actor &actor, object_pose const &player,
  std::span<scenario_actor const> const active, city_map const &cells, resources::geometry_bank const &bank,
  uint8_t const damage_mask, uint16_t frame_step) {
  /// Compose 8823's airborne navigation after its script update; report firing checks for the weapon owner
  if(actor.parameters.update_entry != 0x8823 || actor.definition_slot == 23) {
    throw std::invalid_argument{"Actor requires a different movement callback"};
  }
  actor.previous_position = actor.pose.position;
  auto const &definition{*actor.parameters.definition};
  advance_actor_awareness(actor.awareness, actor.pose, player,
    {.decay{actor.behaviour[2]}, .rise{actor.behaviour[3]}, .strength{actor.behaviour[4]}, .cooldown_shift{definition.role_data[3]}}, frame_step);
  select_actor_target(actor);
  actor_course course;
  if(actor.selected_target & 0x8000) {
    auto const target{std::ranges::find_if(active, [&](auto const &candidate){
      return 0xd986 + candidate.index * 112 == actor.selected_target;
    })};
    course = actor_object_course(actor.pose, target != active.end() && !(target->flags & 0x20) ? target->pose : player);
  } else {
    auto const column{static_cast<uint8_t>(actor.selected_target)};
    auto const row{static_cast<uint8_t>(actor.selected_target >> 8)};
    auto const type{column < 128 && row < 128 ? cells[row * 128 + column].type : 0};
    resources::city_type descriptor{.collision_marker{255}};
    resources::model_header model;
    if(type != 0) {
      descriptor = bank.city_types()[type - 1];
      model = bank.header_at(descriptor.model_offset);
    }
    course = actor_cell_course(actor, actor.selected_target, descriptor, model);
  }
  reset_actor_clearance(actor);
  for(auto const &neighbour : active) consider_actor_clearance(actor, neighbour, course);
  adjust_actor_clearance(actor, course, actor_city_clearance(actor.pose, cells, bank, damage_mask));
  auto const manoeuvre{choose_actor_manoeuvre(actor, course)};
  frame_step = steer_actor(actor.pose, actor.attitude,
    {.response{actor.parameters.angular_response}, .bank_response{actor.parameters.motion[0]},
      .bank_limit{actor.parameters.motion[1]}, .turn_response{actor.parameters.motion[2]}},
    manoeuvre.pitch, manoeuvre.turn_drive, frame_step);
  advance_actor_speed(actor.pose, manoeuvre.speed, definition.role_data[0], definition.role_data[1], frame_step);
  return manoeuvre.firing_distance;
}

} // namespace darker::game
