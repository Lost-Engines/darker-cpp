#include "game/mission_combat.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include <utility>
#include "game/actor_update.h"
#include "game/caero_weapons.h"
#include "game/effect_tables.h"
#include "game/object_definitions.h"
#include "game/projectile_motion.h"
#include "game/projectile_update.h"
#include "game/tunnel_navigation.h"

namespace darker::game {
namespace {

void damage_world_cell(uint8_t const column, uint8_t const row, uint16_t const height,
  city_map &cells, resources::geometry_bank const &bank, effect_system &effects, uint16_t const clock, uint8_t &counter) {
  /// 67BF adds a damage stage, carries the resulting high bit into C221 and emits the type's effect at its origin
  auto &cell{cells.at(row*128 + column)};
  cell.state = static_cast<uint8_t>(cell.state + 32);
  counter = static_cast<uint8_t>(counter + ((cell.state & 128) != 0));
  if(cell.type == 0) return;
  auto const &type{bank.city_types()[cell.type - 1]};
  effects.spawn(building_effect_recipes.at(type.unknown_5),
    {static_cast<uint16_t>(column*256 + type.column_fraction), static_cast<uint16_t>(row*256 + type.row_fraction),height},clock);
}

void impact_projectile_world(city_collision_result const &contact, std::array<uint16_t,3> const impact,
  city_map &cells, resources::geometry_bank const &bank, effect_system &effects, uint16_t const clock, uint16_t const terrain_recipe, uint8_t &counter, uint8_t const damage_mask) {
  /// Share the reconstructed building damage path while preserving each projectile list's terrain recipe
  if(contact.contact == city_contact::building && contact.category == 2) {
    auto &cell{cells[contact.row * 128 + contact.column]};
    auto const model{bank.city_model_offset(cell.type, cell.state, damage_mask)};
    auto const bytes{bank.model_pool()};
    if(bytes[model] != std::byte{0} || bytes[model + 1] != std::byte{0}) {
      damage_world_cell(contact.column,contact.row,impact[2],cells,bank,effects,clock,counter);
    } else effects.spawn(0x7386, impact, clock);
  }
  if(!(contact.contact == city_contact::building && contact.category == 2)) {
    effects.spawn(contact.contact == city_contact::building ? 0x7386 : terrain_recipe, impact, clock);
  }
}

} // namespace

mission_combat::mission_combat(std::vector<scenario_actor> initial) : actors{std::move(initial)} {
  /// Keep stable native identities and reverse source order within the original air/ground/static category traversal
  std::stable_sort(actors.begin(),actors.end(),[](auto const &a, auto const &b){ return a.category < b.category; });
}

void mission_combat::update_difficulty(uint32_t const clock) noexcept {
  /// 3DB1–3DCB increase the scenario's firing pressure after four clock wraps, saturating after eight
  auto const wraps{static_cast<uint8_t>(clock >> 16)};
  if(wraps < 4) return;
  auto const pressure{wraps >= 8 ? 255u : ((clock >> 8) - 1024) >> 2};
  difficulty = std::max(difficulty,static_cast<uint8_t>(pressure));
}

unsigned int mission_combat::remaining_objectives() const noexcept {
  /// C16F counts admitted objective objects until their removal, including falling and effect-only records
  return static_cast<unsigned int>(std::ranges::count_if(actors, [](auto const &actor){ return (actor.attributes & 1) != 0; }));
}

void mission_combat::advance(player_flight &player, city_map &cells, resources::geometry_bank const &bank,
  uint32_t const elapsed_ticks, uint16_t const frame_step, uint16_t const changes, bool const trigger_pressed, std::span<std::byte const> const routes, uint8_t const script_multiplier, tunnel_network const *const network) {
  /// Follow actor scripts and motion, player firing, projectile movement and collision/removal phases
  auto const clock{static_cast<uint16_t>(elapsed_ticks)};
  effects.advance(clock, frame_step);
  player_fired = false;
  player_hit = false;
  auto &caero{std::get<caero_flight_state>(player.craft)};
  auto const player_definition{player.tunnel ? 28u : 25u};
  auto const damage_mask{static_cast<uint8_t>(player.tunnel ? 0x60 : 0x20)};
  auto const player_extent{bank.header_at(bank.special_models()[player_definition]).extent};
  std::erase_if(actors, [&](auto const &actor){
    bool const expired{(actor.flags & 0x60) && std::bit_cast<int16_t>(static_cast<uint16_t>(actor.expiry - clock)) < 0};
    if(expired && (actor.attributes & 1)) ++completed_objectives;
    return expired;
  });
  for(auto &actor : actors) {
    if(actor.parameters.update_entry == 0x6ed3) continue;
    if(actor.parameters.update_entry == 0x8f3b) {
      if(!actor.route) throw std::logic_error{"Ground callback has no vehicle route"};
      actor.previous_position = actor.pose.position;
      auto const event{advance_vehicle_route(*actor.route,actor.pose,actor.flags,routes,clock,bank.header_at(actor.parameters.model_token).height,random_state)};
      if(event.damage_cell) {
        damage_world_cell(static_cast<uint8_t>(*event.damage_cell),static_cast<uint8_t>(*event.damage_cell >> 8),0xe0,cells,bank,effects,clock,world_damage_counter);
      }
      if(event.deadline) actor.expiry = *event.deadline;
      if(event.effect) {
        auto const &effect{*event.effect};
        if(effect.recipe) effects.spawn(effect.recipe,effect.position,clock);
        else effects.spark(effect.position,effect.phase,effect.sound_level,clock);
      }
      continue;
    }
    if(actor.parameters.update_entry == 0) continue;
    if((actor.parameters.update_entry == 0x8823 || actor.parameters.update_entry == 0x8609) && !actor.script.stopped) {
      mission_context context{.program{routes},.cells{cells},.clock{elapsed_ticks},.time_multiplier{script_multiplier}, .object_counter{static_cast<uint8_t>(completed_objectives)},
        .current_cell{static_cast<uint16_t>((actor.pose.position[0] >> 8) | (actor.pose.position[1] & 0xff00))},
        .set_target{[&](uint16_t const target, bool const flag_02){
          actor.target_token = target;
          actor.flags = static_cast<uint8_t>((actor.flags & 0xfd) | (flag_02 ? 2 : 0));
        }}};
      advance_mission_script(actor.script,context);
    }
    if(actor.parameters.update_entry == 0x8daa) advance_falling_aircraft(actor, frame_step);
    else if(actor.parameters.update_entry == 0x8609) {
      if(!network) throw std::logic_error{"Underground actor update requires its route network"};
      advance_tunnel_actor(actor,player.pose(),actors,cells,*network,frame_step);
    } else advance_surface_actor(actor, player.pose(), actors, cells, bank, damage_mask, frame_step,
      [&](scenario_actor &source, actor_course const course, uint8_t const distance){
        auto const shot{fire_skimma_gun(source, player.pose(), player.lifecycle.flags, player_extent, course, distance, clock, changes, random_state)};
        if(shot) effects.gun_impact(shot->end, shot->hit, clock);
        if(shot && shot->hit) {
          apply_player_damage(caero.damage, 0x15, 3, false, false, random_state);
          player_hit = true;
        }
        if(source.selected_target == 0xd986) {
          if(auto const slot{aircraft_projectile_definition(source,player.lifecycle.flags,course,distance,clock,difficulty)}) {
            // CAF0 records the attempt time even if the hostile pool is exhausted.
            source.last_shot = clock;
            auto const &definition{original_object_definitions[*slot]};
            launch_emitter const launcher{.position{source.pose.position},.fractions{source.pose.fractions},
              .angles{source.pose.angles},.speed{source.pose.speed},.side_flags{source.flags},
              .definition_strength{source.parameters.definition->impact_strength}};
            hostile_projectiles.launch({.definition{definition},.emitter{launcher},.model_token{bank.special_models()[*slot]},
              .clock{clock},.lifetime{static_cast<uint16_t>(definition.role_data[1] * 256)},.target_token{source.selected_target}});
          }
        }
      });
    if(auto const severity{damage_trail_severity(actor.awareness.cooldown, actor.flags, changes)}) {
      effects.trail(actor.previous_position, *severity, random_state, clock);
    }
  }
  auto const &pose{player.pose()};
  launch_emitter const emitter{.position{pose.position}, .fractions{pose.fractions}, .angles{pose.angles}, .speed{pose.speed},
    .side_flags{player.lifecycle.flags}, .definition_strength{original_object_definitions[player_definition].impact_strength}};
  weapon_ready = false;
  if(primary_weapon == 1 || primary_weapon == 2) {
    auto const result{fire_pinner(projectiles, caero.energy, emitter, primary_weapon, player.lifecycle.flags, trigger_pressed, bank.special_models()[primary_weapon - 1], clock)};
    weapon_ready = result.ready;
    player_fired = result.shot != nullptr;
    if(result.shot && missile_camera_enabled) camera_projectile = result.shot;
  }
  for(auto *shot{projectiles.objects().head}; shot;) {
    bool const expired{(shot->flags & 8) ? update_projectile_deadline(*shot, clock)
      : update_projectile(*shot, clock, frame_step, &player.pose()) == projectile_update_result::expired};
    if(expired) {
      if(camera_projectile == shot) camera_projectile = nullptr;
      shot = projectiles.recycle(*shot);
      continue;
    }
    shot = shot->next;
  }
  for(auto *shot{hostile_projectiles.objects().head}; shot;) {
    bool const expired{(shot->flags & 8) ? update_projectile_deadline(*shot,clock)
      : update_projectile(*shot,clock,frame_step,&player.pose()) == projectile_update_result::expired};
    if(expired) { shot = hostile_projectiles.recycle(*shot); continue; }
    shot = shot->next;
  }
  for(auto &actor : actors) {
    if((actor.flags & 8) || actor.parameters.update_entry == 0x8f3b) continue;
    auto const contact{sweep_city(bank, cells, damage_mask, actor.previous_position, actor.pose.position, 12, 10)};
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
    auto const contact{sweep_city(bank, cells, damage_mask, shot->previous_position, end, 2, 10)};
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
      auto const reaction{hit_actor(*victim, shot->parameters.definition->impact_strength, clock, random_state)};
      effects.spawn(reaction.effect, reaction.at_actor ? victim->pose.position : impact, clock);
      if(reaction.remove) {
        if(victim->attributes & 1) ++completed_objectives;
        actors.erase(actors.begin() + (victim - actors.data()));
      }
    }
    else impact_projectile_world(contact,impact,cells,bank,effects,clock,0x721c,world_damage_counter,damage_mask);
    shot->flags |= 0x28;
    shot->parameters.update_entry = 0x6ed3;
    shot->deadline = static_cast<uint16_t>(clock + 256);
  }
  for(auto *shot{hostile_projectiles.objects().head}; shot; shot = shot->next) {
    if(shot->flags & 8) continue;
    auto end{shot->placement.position};
    auto const contact{sweep_city(bank,cells,damage_mask,shot->previous_position,end,2,10)};
    bool const hit{!(player.lifecycle.flags & 0x20) && sweep_aircraft(player.pose(),player_extent,2,shot->previous_position,end)};
    if(!hit && contact.contact == city_contact::none) continue;
    shot->placement.position = end;
    if(hit) {
      // 6E95 halves definition strength and derives the angular kick from that amount.
      uint8_t const amount{static_cast<uint8_t>(shot->parameters.definition->impact_strength >> 1)};
      apply_player_damage(caero.damage,amount,static_cast<uint8_t>((amount >> 1) - 7),false,false,random_state);
      effects.spawn(0x70c3,end,clock);
      player_hit = true;
    } else {
      impact_projectile_world(contact,end,cells,bank,effects,clock,0x7199,world_damage_counter,damage_mask);
    }
    shot->flags |= 0x28;
    shot->parameters.update_entry = 0x6ed3;
    shot->deadline = static_cast<uint16_t>(clock + 256);
  }
  if(player_damage_is_lethal(caero.damage) && start_player_crash(player.pose(), player.lifecycle, clock)) player.engine_flags = 0;
}

} // namespace darker::game
