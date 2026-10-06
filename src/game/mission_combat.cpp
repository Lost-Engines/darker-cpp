#include "game/mission_combat.h"
#include <algorithm>
#include <bit>
#include <utility>
#include "game/actor_update.h"
#include "game/caero_weapons.h"
#include "game/effect_tables.h"
#include "game/object_definitions.h"
#include "game/projectile_motion.h"
#include "game/projectile_update.h"

namespace darker::game {

mission_combat::mission_combat(std::vector<scenario_actor> initial) : actors{std::move(initial)} {
  /// Keep stable scenario identities while active traversal follows the source list's reverse order
}

unsigned int mission_combat::remaining_objectives() const noexcept {
  /// C16F counts admitted objective objects until their removal, including falling and effect-only records
  return static_cast<unsigned int>(std::ranges::count_if(actors, [](auto const &actor){ return (actor.attributes & 1) != 0; }));
}

void mission_combat::advance(player_flight &player, city_map &cells, resources::geometry_bank const &bank,
  uint16_t const clock, uint16_t const frame_step, uint16_t const changes, bool const trigger_pressed) {
  /// Follow air callbacks, player firing, projectile movement and collision/removal phases for the first Delphi mission
  effects.advance(clock, frame_step);
  player_fired = false;
  player_hit = false;
  auto &caero{std::get<caero_flight_state>(player.craft)};
  auto const player_extent{bank.header_at(bank.special_models()[25]).extent};
  std::erase_if(actors, [&](auto const &actor){
    bool const expired{(actor.flags & 0x20) && std::bit_cast<int16_t>(static_cast<uint16_t>(actor.expiry - clock)) < 0};
    if(expired && (actor.attributes & 1)) ++completed_objectives;
    return expired;
  });
  for(auto &actor : actors) {
    if(actor.flags & 8) continue;
    if(actor.parameters.update_entry == 0x8daa) advance_falling_aircraft(actor, frame_step);
    else advance_surface_actor(actor, player.pose(), actors, cells, bank, 0x20, frame_step,
      [&](scenario_actor &source, actor_course const course, uint8_t const distance){
        auto const shot{fire_skimma_gun(source, player.pose(), player.lifecycle.flags, player_extent, course, distance, clock, changes, random_state)};
        if(shot) effects.gun_impact(shot->end, shot->hit, clock);
        if(shot && shot->hit) {
          apply_player_damage(caero.damage, 0x15, 3, false, false, random_state);
          player_hit = true;
        }
      });
    if(auto const severity{damage_trail_severity(actor.awareness.cooldown, actor.flags, changes)}) {
      effects.trail(actor.previous_position, *severity, random_state, clock);
    }
  }
  auto const &pose{player.pose()};
  launch_emitter const emitter{.position{pose.position}, .fractions{pose.fractions}, .angles{pose.angles}, .speed{pose.speed},
    .side_flags{player.lifecycle.flags}, .definition_strength{original_object_definitions[25].impact_strength}};
  weapon_ready = false;
  if(primary_weapon == 1) {
    auto const result{fire_pinner_direct(projectiles, caero.energy, emitter, player.lifecycle.flags, trigger_pressed, bank.special_models()[0], clock)};
    weapon_ready = result.ready;
    player_fired = result.shot != nullptr;
  }
  for(auto *shot{projectiles.objects().head}; shot;) {
    bool const expired{(shot->flags & 8) ? update_projectile_deadline(*shot, clock)
      : update_projectile(*shot, clock, frame_step) == projectile_update_result::expired};
    if(expired) { shot = projectiles.recycle(*shot); continue; }
    shot = shot->next;
  }
  for(auto &actor : actors) {
    if(actor.flags & 8) continue;
    auto const contact{sweep_city(bank, cells, 0x20, actor.previous_position, actor.pose.position, 12, 10)};
    if(contact.contact != city_contact::none) {
      effects.spawn(contact.contact == city_contact::building ? 0x716c : 0x7199, actor.pose.position, clock);
      actor.flags |= 0x28;
      actor.parameters.update_entry = 0x6ed3;
      actor.expiry = static_cast<uint16_t>(clock + 256);
    }
  }
  for(auto *shot{projectiles.objects().head}; shot; shot = shot->next) {
    if(shot->flags & 8) continue;
    auto end{shot->placement.position};
    auto const contact{sweep_city(bank, cells, 0x20, shot->previous_position, end, 2, 10)};
    scenario_actor *victim{nullptr};
    auto impact{end};
    for(auto &actor : actors) {
      auto candidate{end};
      if(sweep_aircraft(actor.pose, bank.header_at(actor.parameters.model_token).extent, 2, shot->previous_position, candidate)) {
        victim = &actor;
        impact = candidate;
      }
    }
    if(!victim && contact.contact == city_contact::none) continue;
    shot->placement.position = impact;
    if(victim) {
      auto const reaction{hit_aircraft(*victim, shot->parameters.definition->impact_strength, clock, random_state)};
      effects.spawn(reaction == impact_effect::fatal ? 0x7319 : 0x72df, impact, clock);
    }
    else if(contact.contact == city_contact::building && contact.category == 2) {
      auto &cell{cells[contact.row * 128 + contact.column]};
      auto const model{bank.city_model_offset(cell.type, cell.state, 0x20)};
      auto const bytes{bank.model_pool()};
      if(bytes[model] != std::byte{0} || bytes[model + 1] != std::byte{0}) {
        cell.state = static_cast<uint8_t>(cell.state + 32);
        auto const &type{bank.city_types()[cell.type - 1]};
        effects.spawn(building_effect_recipes.at(type.unknown_5),
          {static_cast<uint16_t>(contact.column * 256 + type.column_fraction), static_cast<uint16_t>(contact.row * 256 + type.row_fraction), impact[2]}, clock);
      } else effects.spawn(0x7386, impact, clock);
    }
    if(!victim && !(contact.contact == city_contact::building && contact.category == 2)) {
      effects.spawn(contact.contact == city_contact::building ? 0x7386 : 0x721c, impact, clock);
    }
    shot->flags |= 0x28;
    shot->parameters.update_entry = 0x6ed3;
    shot->deadline = static_cast<uint16_t>(clock + 256);
  }
  if(player_damage_is_lethal(caero.damage) && start_player_crash(player.pose(), player.lifecycle, clock)) player.engine_flags = 0;
}

} // namespace darker::game
