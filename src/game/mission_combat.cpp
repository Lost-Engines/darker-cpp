#include "game/mission_combat.h"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include "game/actor_activation.h"
#include "game/actor_update.h"
#include "game/building_impact.h"
#include "game/caero_weapons.h"
#include "game/dual_launch.h"
#include "game/effect_tables.h"
#include "game/object_deadline.h"
#include "game/object_definitions.h"
#include "game/objective_counter.h"
#include "game/projectile_motion.h"
#include "game/projectile_update.h"
#include "game/tunnel_navigation.h"
#include "game/vehicle_combat.h"

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

void impact_projectile_world(city_collision_result const &contact, std::array<uint16_t,3> const impact, object_definition const &definition,
  city_map &cells, resources::geometry_bank const &bank, effect_system &effects, uint16_t const clock, uint16_t const terrain_recipe, uint8_t &counter, uint8_t const damage_mask) {
  /// CDB9 distinguishes marked player targets, hostile bombing and universal category-two breakable components
  if(contact.contact != city_contact::building) { effects.spawn(terrain_recipe,impact,clock); return; }
  auto const &cell{cells[contact.row*128+contact.column]};
  auto const model{bank.city_model_offset(cell.type,cell.state,damage_mask)};
  auto const bytes{bank.model_pool()};
  bool const linked{bytes[model] != std::byte{0} || bytes[model+1] != std::byte{0}};
  auto const found{std::ranges::find_if(original_object_definitions,[&](auto const &original){ return &original == &definition; })};
  auto const slot{static_cast<size_t>(found-original_object_definitions.begin())};
  if(projectile_damages_building(slot,contact.category,cell.state,linked)) {
    damage_world_cell(contact.column,contact.row,impact[2],cells,bank,effects,clock,counter);
  } else effects.spawn(0x7386,impact,clock);
}

} // namespace

mission_combat::mission_combat(std::vector<scenario_actor> initial) : actors{std::move(initial)} {
  /// Keep stable native identities and reverse source order within the original air/ground/static category traversal
  std::stable_sort(actors.begin(),actors.end(),[](auto const &a, auto const &b){ return a.category < b.category; });
  outstanding_objectives = static_cast<uint8_t>(std::ranges::count_if(actors,[](auto const &actor){ return actor.attributes & 1; }));
}

std::span<uint8_t const> mission_combat::status_flags(uint8_t const player_flags) noexcept {
  /// C1EF addresses stable object indices, including the player and records removed from active lists
  retained_flags[0] = player_flags;
  for(auto const *group : {&actors,&reserves,&free_actors}) {
    for(auto const &actor : *group) retained_flags[actor.index] = actor.flags;
  }
  return retained_flags;
}

void mission_combat::release_target(uint16_t const token) noexcept {
  /// 7A52 makes missiles targeting a removed record self-guiding and releases its selected lock
  for(auto *pool : {&projectiles,&hostile_projectiles}) {
    for(auto *shot{pool->objects().head}; shot; shot = shot->next) {
      if(shot->target_token == token) shot->target_token = shot->native_id;
    }
  }
  if(target.token == token) target.clear();
}

void mission_combat::spawn_aircraft(player_flight const &player, city_map const &cells, resources::geometry_bank const &bank, uint16_t const clock, uint16_t const frame_step) {
  /// 3E02 visits occupied warehouses before the player and other moving-object callbacks
  if(!player.tunnel) advance_aircraft_spawning(spawning,actors,free_actors,cells,bank,player.pose(),clock,frame_step,random_state);
}

void mission_combat::collide_aircraft(city_map const &cells, resources::geometry_bank const &bank, uint8_t const damage_mask, uint16_t const clock, bool const underground) {
  /// 6DF1 clips each airborne owner against the city and its own list; 6E6D applies strengths 40h and 60h to the pair
  std::array<uint8_t,256> owners{};
  size_t count{0};
  for(auto const &actor : actors) if(actor.category == actor_category::air) owners[count++] = actor.index;
  auto const find{[&](uint8_t const index){ return std::ranges::find(actors,index,&scenario_actor::index); }};
  auto const remove{[&](uint8_t const index){
    auto const actor{find(index)};
    if(actor == actors.end()) return;
    retained_flags[index] = static_cast<uint8_t>(actor->flags | 0x20);
    release_target(static_cast<uint16_t>(0xd986+index*112));
    completed_objectives += actor->attributes & 1;
    adjust_objectives(static_cast<uint8_t>(-(actor->attributes & 1)));
    actors.erase(actor);
  }};
  for(auto const index : std::span{owners}.first(count)) {
    auto const owner{find(index)};
    if(owner == actors.end() || (owner->flags & 0x10)) continue;
    auto end{owner->pose.position};
    auto const contact{sweep_city(bank,cells,damage_mask,owner->previous_position,end,12,10)};
    scenario_actor *victim{nullptr};
    for(auto &actor : actors) {
      if(actor.category != actor_category::air || actor.index == index) continue;
      auto candidate{end};
      if(sweep_aircraft(actor.pose,bank.header_at(actor.parameters.model_token).extent,12,owner->previous_position,candidate)) victim = &actor;
    }
    if(victim) {
      end[2] &= 0xfff8;
      owner->pose.position = end;
      auto const victim_index{victim->index};
      auto const first{hit_actor(*owner,0x40,clock,random_state,underground)};
      effects.spawn(first.effect,end,clock);
      auto const second{hit_actor(*victim,0x60,clock,random_state,underground)};
      effects.spawn(second.effect,second.at_actor ? victim->pose.position : end,clock);
      if(first.remove) remove(index);
      if(second.remove) remove(victim_index);
    } else if(contact.contact != city_contact::none) {
      owner->pose.position = end;
      if(owner->flags & 8) continue;
      effects.spawn(contact.contact == city_contact::building ? 0x716c : 0x7199,end,clock);
      owner->flags |= 0x28;
      owner->parameters.update_entry = 0x6ed3;
      owner->expiry = static_cast<uint16_t>(clock+256);
    }
  }
}

void mission_combat::detonate_dual_launch(projectile &shot, uint16_t const clock, bool const underground) {
  /// CBE7 applies separate aircraft, ground and stationary blast passes before retiring both paired projectiles
  for(auto const category : {actor_category::air,actor_category::ground,actor_category::stationary}) {
    for(auto actor{actors.begin()}; actor != actors.end();) {
      auto const strength{actor->category == category ? dual_launch_impact(shot.placement,actor->pose,category == actor_category::air) : std::nullopt};
      if(!strength) { ++actor; continue; }
      auto const reaction{hit_actor(*actor,*strength,clock,random_state,underground)};
      effects.spawn(reaction.effect,actor->pose.position,clock);
      if(!reaction.remove) { ++actor; continue; }
      retained_flags[actor->index] = static_cast<uint8_t>(actor->flags | 0x20);
      release_target(static_cast<uint16_t>(0xd986+actor->index*112));
      completed_objectives += actor->attributes & 1;
      adjust_objectives(static_cast<uint8_t>(-(actor->attributes & 1)));
      actor = actors.erase(actor);
    }
  }
  auto *capsule{projectiles.resolve(shot.target_token)};
  if(!capsule) throw std::logic_error{"Dual Launch detonation lost its paired capsule"};
  effects.spawn(0x71e8,capsule->placement.position,clock);
  for(auto *part : {&shot,capsule}) {
    part->parameters.update_entry = 0x6ed3;
    part->flags |= 0x28;
    part->deadline = static_cast<uint16_t>(clock+256);
  }
}

void mission_combat::activate_reserves(actor_category const category, uint8_t const count, object_pose const &player, uint16_t const clock) {
  /// C37A adds each admitted record's objective bit without the signed clamp used by removal
  auto const counted{[&]{ return std::ranges::count_if(actors,[](auto const &actor){ return actor.attributes & 1; }); }};
  auto const before{counted()};
  activate_scenario_reserves(actors,reserves,category,count,player,clock);
  outstanding_objectives = static_cast<uint8_t>(outstanding_objectives + counted() - before);
}

void mission_combat::adjust_objectives(uint8_t const operand) noexcept {
  /// Script 33 and object removal share C16E's mutable outstanding counter
  outstanding_objectives = adjust_objective_counter(outstanding_objectives,operand);
}

void mission_combat::update_difficulty(uint32_t const clock) noexcept {
  /// 3DB1–3DCB increase the scenario's firing pressure after four clock wraps, saturating after eight
  auto const wraps{static_cast<uint8_t>(clock >> 16)};
  if(wraps < 4) return;
  auto const pressure{wraps >= 8 ? 255u : ((clock >> 8) - 1024) >> 2};
  difficulty = std::max(difficulty,static_cast<uint8_t>(pressure));
}

unsigned int mission_combat::remaining_objectives() const noexcept {
  /// C16F is independently mutable: withdrawal scripts can clear obligations while aircraft remain
  return outstanding_objectives;
}

void mission_combat::advance(player_flight &player, city_map &cells, resources::geometry_bank const &bank,
  uint32_t const elapsed_ticks, uint16_t const frame_step, uint16_t const changes, bool const trigger_pressed, std::span<std::byte const> const routes, uint8_t const script_multiplier, tunnel_network const *const network, bool const secondary_pressed, bool const secondary_held) {
  /// Follow actor scripts and motion, player firing, projectile movement and collision/removal phases
  auto const clock{static_cast<uint16_t>(elapsed_ticks)};
  effects.advance(clock, frame_step);
  player_fired = false;
  if(player.lifecycle.crashing) weapon_charge = 0;
  player_hit = false;
  auto &caero{std::get<caero_flight_state>(player.craft)};
  auto const player_definition{player.tunnel ? 28u : 25u};
  auto const damage_mask{static_cast<uint8_t>(player.tunnel ? 0x60 : 0x20)};
  auto const player_extent{bank.header_at(bank.special_models()[player_definition]).extent};
  std::erase_if(actors, [&](auto &actor){
    if(!advance_object_deadline(actor.flags,actor.expiry,actor.fade,actor.pose.position[2],clock)) return false;
    release_target(static_cast<uint16_t>(0xd986 + actor.index*112));
    completed_objectives += actor.attributes & 1;
    adjust_objectives(static_cast<uint8_t>(-(actor.attributes & 1)));
    auto const lifetime{static_cast<uint8_t>(actor.attributes & 0xfe)};
    actor.flags |= 0x20;
    retained_flags[actor.index] = actor.flags;
    if(lifetime && actor.category == actor_category::air) {
      actor.attributes = lifetime < 0xfe ? static_cast<uint8_t>(lifetime - 2) : lifetime;
      free_actors.insert(free_actors.begin(),actor);
    }
    return true;
  });
  for(auto &actor : actors) {
    if(actor.parameters.update_entry == 0x6ed3) continue;
    if(actor.parameters.update_entry == 0x8f3b) {
      if(!actor.route) throw std::logic_error{"Ground callback has no vehicle route"};
      actor.previous_position = actor.pose.position;
      auto const event{advance_vehicle_route(*actor.route,actor.pose,actor.flags,routes,clock,bank.header_at(actor.parameters.model_token).height,random_state)};
      if(event.firing_direction) fire_vehicle_missile(hostile_projectiles,actor,player.pose(),cells,bank.city_types(),
        *event.firing_direction,clock,difficulty,bank.special_models()[18]);
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
    auto const callback{actor.parameters.update_entry};
    if((actor.parameters.update_entry == 0x8823 || actor.parameters.update_entry == 0x8609) && !actor.script.stopped) {
      mission_context context{.program{routes},.object_flags{status_flags(player.lifecycle.flags)},.cells{cells},.clock{elapsed_ticks},.time_multiplier{script_multiplier}, .object_counter{static_cast<uint8_t>(completed_objectives)},
        .current_cell{static_cast<uint16_t>((actor.pose.position[0] >> 8) | (actor.pose.position[1] & 0xff00))},
        .set_target{[&](uint16_t const target, bool const flag_02){
          actor.target_token = target;
          actor.flags = static_cast<uint8_t>((actor.flags & 0xfd) | (flag_02 ? 2 : 0));
        }},
        .retire_distant_actor{[&]{ return darker::game::retire_distant_actor(actor,player.pose(),clock); }}};
      context.adjust_objectives = [&](uint8_t const operand){ adjust_objectives(operand); return remaining_objectives() == 0; };
      context.set_tunnel_oscillation = [&](uint8_t const phase){
        if(!actor.tunnel) throw std::logic_error{"Tunnel direction command requires an underground actor"};
        actor.tunnel->oscillation = phase;
      };
      context.register_owner = [&]{ return std::exchange(script_owner,static_cast<uint16_t>(0xd986 + actor.index*112)); };
      advance_mission_script(actor.script,context);
    }
    if(callback == 0x8daa) advance_falling_aircraft(actor, frame_step);
    else if(callback == 0x8ddd) advance_aircraft_departure(actor,clock,frame_step);
    else if(callback == 0x8609) {
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
        if(source.selected_target == 0xd986 || !(source.selected_target & 0x8000)) {
          if(auto const slot{aircraft_projectile_definition(source,player.lifecycle.flags,course,distance,clock,difficulty,building_attacks)}) {
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
      },[&](scenario_actor &source){ drop_aircraft_bomb(hostile_projectiles,source,building_attacks,clock,bank.special_models()[14]); });
    if(auto const severity{damage_trail_severity(actor.awareness.cooldown, actor.flags, changes)}) {
      effects.trail(actor.previous_position, *severity, random_state, clock);
    }
  }
  auto const &pose{player.pose()};
  launch_emitter const emitter{.position{pose.position}, .fractions{pose.fractions}, .angles{pose.angles}, .speed{pose.speed},
    .side_flags{player.lifecycle.flags}, .definition_strength{original_object_definitions[player_definition].impact_strength}};
  weapon_ready = false;
  if(primary_weapon == 1 || primary_weapon == 2 || primary_weapon == 3 || primary_weapon == 7) {
    auto const result{fire_caero_weapon(projectiles,caero.energy,weapon_charge,{.emitter{emitter},.selection{primary_weapon},
      .player_flags{player.lifecycle.flags},.pressed{trigger_pressed},.model{bank.special_models()[primary_weapon - 1]},
      .clock{clock},.frame_step{frame_step},.underground{player.tunnel.has_value()}})};
    weapon_ready = result.ready;
    if(result.next_selection) primary_weapon = result.next_selection;
    player_fired = result.shot != nullptr;
    if(result.shot && missile_camera_enabled) camera_projectile = result.shot;
  }
  secondary_ready = false;
  if(secondary_weapon == 6 || secondary_weapon == 9 || secondary_weapon == 10) {
    auto const result{fire_caero_weapon(projectiles,caero.energy,weapon_charge,{.emitter{emitter},.selection{secondary_weapon},
      .player_flags{player.lifecycle.flags},.pressed{secondary_pressed},.held{secondary_held},.model{bank.special_models()[secondary_weapon - 1]},
      .clock{clock},.frame_step{frame_step},.target{target.token},.underground{player.tunnel.has_value()}})};
    secondary_ready = result.ready;
    player_fired |= result.shot != nullptr;
    if(result.shot && missile_camera_enabled) camera_projectile = result.shot;
  }
  auto const resolve_target{[&](projectile &shot)->projectile_target {
    if(shot.parameters.update_entry == 0xcbce) return &player.pose();
    if(shot.parameters.update_entry != 0xcc61 && shot.parameters.update_entry != 0xcc68 && shot.parameters.update_entry != 0xcbe7) return {};
    if(shot.target_token == 0xd986) return &player.pose();
    if(!(shot.target_token & 0x8000)) return resolve_map_guidance(shot.target_token,cells,bank,damage_mask);
    if(shot.target_token == shot.native_id) return &shot.placement;
    auto const actor{std::ranges::find_if(actors,[&](auto const &candidate){ return 0xd986 + candidate.index*112 == shot.target_token; })};
    if(actor != actors.end()) return &actor->pose;
    if(auto const *other{projectiles.resolve(shot.target_token)}) return &other->placement;
    if(auto const *other{hostile_projectiles.resolve(shot.target_token)}) return &other->placement;
    throw std::logic_error{"Guided projectile target has no active object record"};
  }};
  for(auto *shot{projectiles.objects().head}; shot;) {
    auto const destination{resolve_target(*shot)};
    auto const *paired{std::get_if<object_pose const *>(&destination)};
    auto const separation{shot->parameters.update_entry == 0xcbe7 && paired && *paired && *paired != &shot->placement
      ? std::optional<uint16_t>{dual_launch_separation(shot->placement,**paired)} : std::nullopt};
    auto const result{(shot->flags & 8) ? (update_projectile_deadline(*shot,clock) ? projectile_update_result::expired : projectile_update_result::advanced)
      : update_projectile(*shot,clock,frame_step,destination)};
    if(result == projectile_update_result::expired) {
      release_target(shot->native_id);
      if(camera_projectile == shot) camera_projectile = nullptr;
      shot = projectiles.recycle(*shot);
      continue;
    }
    if(result == projectile_update_result::detonated) detonate_dual_launch(*shot,clock,player.tunnel.has_value());
    else if(separation) dual_launch_pitch = static_cast<uint16_t>((0x80c-std::min<uint16_t>(*separation,0xcd)) >> 2);
    shot = shot->next;
  }
  for(auto *shot{hostile_projectiles.objects().head}; shot;) {

    bool const expired{(shot->flags & 8) ? update_projectile_deadline(*shot,clock)
      : update_projectile(*shot,clock,frame_step,resolve_target(*shot)) == projectile_update_result::expired};
    if(expired) { release_target(shot->native_id); shot = hostile_projectiles.recycle(*shot); continue; }
    shot = shot->next;
  }
  if(!secondary_weapon) target.clear();
  else {
    if(target.token == 0xffff) target.token = acquire_caero_target(player.pose(),actors,cells,bank,damage_mask);
    if(target.token != 0xffff) {
      if(target.token & 0x8000) {
        auto const found{std::ranges::find_if(actors,[&](auto const &actor){ return 0xd986 + actor.index*112 == target.token; })};
        if(found == actors.end()) target.clear();
        else project_caero_target(target,pose.position,found->pose.position,bank.header_at(found->parameters.model_token).extent,targeting_basis,secondary_weapon);
      } else {
        auto const aim{resolve_map_guidance(target.token,cells,bank,damage_mask)};
        auto const cell{cells[(target.token >> 8)*128+(target.token & 127)]};
        project_caero_target(target,pose.position,{aim.position[0],aim.position[1],aim.height},aim.height_extent,targeting_basis,secondary_weapon,cell.type,cell.state);
      }
    }
  }
  if(player.lifecycle.flags & 0x10) target.clear();
  collide_aircraft(cells,bank,damage_mask,clock,player.tunnel.has_value());
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
    auto const strength{shot->parameters.definition == &original_object_definitions[8]
      ? chargeable_impact_strength(shot->deadline,clock) : std::optional<uint8_t>{shot->parameters.definition == &original_object_definitions[0]
        ? pinner_direct_strength(player.tunnel.has_value()) : shot->parameters.definition->impact_strength}};
    if(victim && strength) {
      auto const reaction{hit_actor(*victim, *strength, clock, random_state,player.tunnel.has_value())};
      effects.spawn(reaction.effect, reaction.at_actor ? victim->pose.position : impact, clock);
      if(reaction.remove) {
        retained_flags[victim->index] = static_cast<uint8_t>(victim->flags | 0x20);
        release_target(static_cast<uint16_t>(0xd986 + victim->index*112));
        completed_objectives += victim->attributes & 1;
        adjust_objectives(static_cast<uint8_t>(-(victim->attributes & 1)));
        actors.erase(actors.begin() + (victim - actors.data()));
      }
    }
    else if(!victim) impact_projectile_world(contact,impact,*shot->parameters.definition,cells,bank,effects,clock,player.tunnel ? 0x7386 : 0x721c,world_damage_counter,damage_mask);
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
      impact_projectile_world(contact,end,*shot->parameters.definition,cells,bank,effects,clock,0x7199,world_damage_counter,damage_mask);
    }
    shot->flags |= 0x28;
    shot->parameters.update_entry = 0x6ed3;
    shot->deadline = static_cast<uint16_t>(clock + 256);
  }
  if(player_damage_is_lethal(caero.damage) && start_player_crash(player.pose(), player.lifecycle, clock)) player.engine_flags = 0;
}

} // namespace darker::game
