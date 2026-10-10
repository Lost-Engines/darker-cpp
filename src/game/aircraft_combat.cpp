#include "game/aircraft_combat.h"
#include <bit>
#include "game/angular_motion.h"
#include "game/collision_sweep.h"
#include "game/object_definitions.h"
#include "game/object_impact.h"
#include "game/skimma_weapons.h"

namespace darker::game {

bool sweep_aircraft(object_pose const &target, uint16_t const extent, uint16_t const expansion,
  std::array<uint16_t, 3> const &start, std::array<uint16_t, 3> &end) noexcept {
  /// 6D7B/60C8 use a model-extent cube with altitude divided by eight, independently of model orientation
  auto const radius{static_cast<uint16_t>((extent >> 2) + expansion)};
  auto centre{target.position};
  centre[2] = static_cast<uint16_t>(std::bit_cast<int16_t>(centre[2]) >> 3);
  collision_box box;
  for(size_t axis{0}; axis < 3; ++axis) {
    box.minimum[axis] = static_cast<uint16_t>(centre[axis] - radius);
    box.maximum[axis] = static_cast<uint16_t>(centre[axis] + radius);
  }
  auto previous{start};
  auto next{end};
  previous[2] = static_cast<uint16_t>(std::bit_cast<int16_t>(previous[2]) >> 3);
  next[2] = static_cast<uint16_t>(std::bit_cast<int16_t>(next[2]) >> 3);
  if(!sweep_collision_box(box, previous, next)) return false;
  end = next;
  end[2] = static_cast<uint16_t>(next[2] << 3);
  return true;
}

scenario_actor *sweep_actor_groups(std::span<scenario_actor> const actors, resources::geometry_bank const &bank,
  std::array<uint16_t,3> const &start, std::array<uint16_t,3> const &end, uint16_t const expansion,
  std::span<actor_category const> const categories, std::optional<uint8_t> const excluded) {
  /// 6D60 retains the last intersecting object in list order without shortening the city-clipped sweep
  scenario_actor *result{nullptr};
  for(auto const category : categories) {
    for(auto &actor : actors) {
      if(actor.category != category || (excluded && actor.index == *excluded)) continue;
      auto candidate{end};
      if(sweep_aircraft(actor.pose,bank.header_at(actor.parameters.model_token).extent,expansion,start,candidate)) result = &actor;
    }
  }
  return result;
}

actor_impact_result hit_actor(scenario_actor &actor, uint8_t const strength, uint16_t const clock, uint16_t &random_state, bool const underground) {
  /// CE26 dispatches static removal and zero-resistance effects before CE38's ordinary aircraft damage
  if(actor.parameters.update_entry == 0) return {.effect{0x7296}, .at_actor{true}, .remove{true}};
  if(actor.parameters.definition->impact_strength == 0) {
    if(actor.parameters.definition->role_data[7] & 2) return {.effect{0x721c}};
    actor.expiry = static_cast<uint16_t>(clock + 256);
    actor.flags |= 0x20;
    return {.effect{0x7247}, .at_actor{true}};
  }
  object_impact_state state{
    .rotation{actor.attitude.pitch_rate, actor.attitude.bank_rate},
    .impact_accumulator{actor.awareness.level}, .damage{actor.awareness.cooldown},
    .update_entry{actor.parameters.update_entry}, .deadline{actor.expiry}, .flags{actor.flags},
  };
  auto const result{apply_object_impact(state, strength, actor.parameters.definition->impact_strength, underground, clock, random_state)};
  actor.attitude = {state.rotation.pitch, state.rotation.turn};
  actor.awareness = {state.impact_accumulator, state.damage};
  actor.parameters.update_entry = state.update_entry;
  actor.expiry = state.deadline;
  actor.flags = state.flags;
  return {.effect{static_cast<uint16_t>(result)}};
}

void advance_falling_aircraft(scenario_actor &actor, uint16_t frame_step) noexcept {
  /// 8DAA damps bank motion, pitches down according to bank and approaches the falling speed
  actor.previous_position = actor.pose.position;
  auto const bank{integrate_angular_rate(actor.attitude.bank_rate, 0, frame_step)};
  actor.attitude.bank_rate = bank.rate;
  actor.pose.angles.roll = static_cast<uint16_t>(actor.pose.angles.roll + bank.angle_delta);
  frame_step = bank.frame_step;
  auto const middle{static_cast<uint16_t>(actor.pose.angles.roll - (std::bit_cast<int16_t>(bank.angle_delta) >> 1))};
  auto const signed_bank{std::bit_cast<int16_t>(fold_bank_angle(middle))};
  auto const half{static_cast<uint16_t>(signed_bank ^ (signed_bank < 0 ? -1 : 0)) >> 1};
  auto const target{static_cast<uint16_t>(0xe800 - half - (half >> 2))};
  auto const pitch{calculate_angular_response(static_cast<uint16_t>(target - actor.pose.angles.pitch), actor.attitude.pitch_rate, 32, frame_step)};
  actor.attitude.pitch_rate = pitch.rate;
  actor.pose.angles.pitch = static_cast<uint16_t>(actor.pose.angles.pitch + pitch.angle_delta);
  advance_actor_speed(actor.pose, 17, 64, 223, pitch.frame_step);
}

std::optional<uint8_t> aircraft_projectile_definition(scenario_actor const &actor, uint8_t const target_flags,
  actor_course const course, uint8_t const distance, uint16_t const clock, uint8_t const difficulty, bool const building_attacks) {
  /// 8AFD selects object missiles or the adjacent building-attack definition before the common aim and cooldown checks
  if(actor.selected_target != 0xd986 && (actor.flags & 2)) return std::nullopt;
  bool const building{!(actor.selected_target & 0x8000)};
  if(building ? (!building_attacks || distance >= 10) : (target_flags & 0x30)) return std::nullopt;
  auto const speed{actor.parameters.definition->base_speed};
  auto const pitch_error{static_cast<uint8_t>((static_cast<uint16_t>(course.pitch - actor.pose.angles.pitch) >> 8) + speed)};
  if(pitch_error >= static_cast<uint8_t>(speed * 2)) return std::nullopt;
  auto const heading_error{static_cast<uint8_t>((static_cast<uint16_t>(course.heading - actor.pose.angles.heading) >> 8) + distance)};
  if(heading_error >= static_cast<uint8_t>(distance * 2)) return std::nullopt;
  if(!building && actor.definition_slot == 19 && distance < 8 && actor.behaviour[0] == 0) return std::nullopt;
  auto const slot{static_cast<uint8_t>(actor.parameters.definition->role_data[6] + (building ? 1 : 0))};
  auto const &weapon{original_object_definitions.at(slot)};
  unsigned int const sum{((difficulty >> 1) | 0x80u) + actor.behaviour[0]};
  uint16_t const bias{static_cast<uint16_t>(((sum > 255 ? 0xfd : 0xfc) << 8) | (sum & 255))};
  uint16_t const delay{static_cast<uint16_t>((weapon.role_data[0] * 4 - bias) * 4)};
  if(static_cast<uint16_t>(clock - actor.last_shot) < delay) return std::nullopt;
  return slot;
}

projectile *drop_aircraft_bomb(projectile_pool &pool, scenario_actor &actor, bool const enabled, uint16_t const clock, uint16_t const model_token) {
  /// 8BE6 admits a building-target drop every six timer pages, recording even an unsuccessful allocation attempt
  if((actor.flags & 2) || !enabled || static_cast<uint16_t>(clock - actor.last_shot) < 0x600 || (actor.selected_target & 0x8000)) return nullptr;
  actor.last_shot = clock;
  auto const &definition{original_object_definitions[14]};
  launch_emitter const emitter{.position{actor.pose.position},.fractions{actor.pose.fractions},.angles{actor.pose.angles},
    .speed{actor.pose.speed},.side_flags{actor.flags},.definition_strength{actor.parameters.definition->impact_strength}};
  auto *shot{pool.launch({.definition{definition},.emitter{emitter},.model_token{model_token},.clock{clock},
    .lifetime{static_cast<uint16_t>(definition.role_data[1]*256)},.target_token{actor.selected_target}})};
  if(shot) shot->placement.angles.pitch = 0xc800;
  return shot;
}

std::optional<gun_trace> fire_skimma_gun(scenario_actor const &actor, object_pose const &player, uint8_t const player_flags,
  uint16_t const player_extent, actor_course const course, uint8_t const distance, uint16_t const clock, uint16_t const changes, uint16_t &random_state, uint8_t const target_protection_mask) {
  /// 8B65's slot-19 close-range gun tests the original DX aim bounds and timer bits, then traces a randomised ray
  // 8C28 doubles DH before 8B7C compares it with 16; behaviour byte 50 only controls the later projectile branch.
  if(actor.definition_slot != 19 || distance >= 8) return std::nullopt;
  if(actor.selected_target != 0xd986 || (player_flags & target_protection_mask)) return std::nullopt;
  auto const speed{actor.parameters.definition->base_speed};
  auto const pitch_error{static_cast<uint8_t>((static_cast<uint16_t>(course.pitch - actor.pose.angles.pitch) >> 8) + speed)};
  if(pitch_error >= static_cast<uint8_t>(speed * 2)) return std::nullopt;
  auto const heading_error{static_cast<uint8_t>((static_cast<uint16_t>(course.heading - actor.pose.angles.heading) >> 8) + distance)};
  if(heading_error >= static_cast<uint8_t>(distance * 2)) return std::nullopt;
  auto const elapsed{static_cast<uint16_t>(clock - actor.last_shot)};
  if((elapsed & 0x100) || !(changes & 0x80)) return std::nullopt;
  gun_trace result{.start{actor.pose.position},.end{skimma_gun_endpoint(actor.pose,0,random_state)}};
  result.hit = sweep_aircraft(player, player_extent, 10, result.start, result.end);
  return result;
}

} // namespace darker::game
