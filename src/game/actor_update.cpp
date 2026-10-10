#include "game/actor_update.h"
#include <algorithm>
#include "game/city_map.h"
#include "game/native_object_layout.h"
#include "game/target_reference.h"

namespace darker::game {

std::optional<uint8_t> advance_surface_actor(scenario_actor &actor, object_pose const &player,
  std::span<scenario_actor const> const active, city_map const &cells, resources::geometry_bank const &bank,
  uint8_t const damage_mask, game_duration frame_step, std::function<void(scenario_actor&, actor_course, uint8_t)> const &fire, std::function<void(scenario_actor&)> const &drop, std::array<uint8_t, 4> *const threat_errors) {
  /// Compose 8823's airborne navigation after its script update; report firing checks for the weapon owner
  actor.previous_position = actor.pose.position;
  auto const &definition{*actor.parameters.definition};
  advance_actor_awareness(actor.awareness, actor.pose, player,
    {
      .decay{actor.behaviour.awareness_decay},
      .rise{actor.behaviour.awareness_rise},
      .strength{actor.behaviour.awareness_strength},
      .cooldown_shift{definition.role_data.craft().cooldown_shift}
    }, frame_step);
  select_actor_target(actor);
  actor_course course;
  if(target_reference{actor.selected_target}.is_object_encoded()) {
    auto const target{std::ranges::find_if(active, [&](auto const &candidate){
      return native_object_layout::actor(candidate.index) == actor.selected_target;
    })};
    course = actor_object_course(actor.pose, target != active.end() && !(target->flags & 0x20) ? target->pose : player);
  } else {
    auto const column{static_cast<uint8_t>(actor.selected_target)};
    auto const row{static_cast<uint8_t>(actor.selected_target >> 8)};
    auto const type{column < city_map_size.column && row < city_map_size.row ? cells[city_cell_index(column, row)].type : 0};
    resources::city_type descriptor{
      .collision_marker{resources::city_type::background_marker}
    };
    resources::model_header model;
    if(type != 0) {
      descriptor = bank.city_types()[type - 1];
      model = bank.header_at(descriptor.model_offset);
    }
    course = actor_cell_course(actor, actor.selected_target, descriptor, model);
  }
  if(actor.definition_slot == 23 && course.distance < 100 && drop) drop(actor);
  auto const firing_course{course};
  reset_actor_clearance(actor);
  for(auto const &neighbour : active) {
    if(neighbour.category == actor_category::air) consider_actor_clearance(actor, neighbour, course);
  }
  adjust_actor_clearance(actor, course, actor_city_clearance(actor.pose, cells, bank, damage_mask));
  auto const manoeuvre{choose_actor_manoeuvre(actor, course)};
  if(manoeuvre.firing_distance && fire) fire(actor, firing_course, *manoeuvre.firing_distance);
  frame_step = steer_actor(actor.pose, actor.attitude,
    {
      .response{actor.parameters.angular_response},
      .bank_response{actor.parameters.motion.bank_response},
      .bank_limit{actor.parameters.motion.bank_limit},
      .turn_response{actor.parameters.motion.turn_response}
    },
    manoeuvre.pitch, manoeuvre.turn_drive, frame_step);
  advance_actor_speed(actor.pose, manoeuvre.speed, definition.role_data.craft().acceleration, definition.role_data.craft().deceleration, frame_step);
  if(threat_errors) consider_aircraft_threat(*threat_errors, actor, firing_course);
  return manoeuvre.firing_distance;
}

} // namespace darker::game
