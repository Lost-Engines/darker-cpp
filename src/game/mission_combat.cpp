#include "game/mission_combat.h"
#include "game/player_flags.h"
#include "game/weapon_selection.h"
#include <algorithm>
#include <bit>
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
#include "game/random.h"
#include "game/skimma_weapons.h"
#include "game/tunnel_navigation.h"
#include "game/vehicle_combat.h"
#include "maths/world_coordinates.h"
#include "game/native_object_layout.h"
#include "game/object_catalogue.h"
#include "game/target_reference.h"

namespace darker::game {
namespace {

std::array constexpr actor_collision_groups{actor_category::ground, actor_category::stationary, actor_category::air};
std::array constexpr aircraft_collision_group{actor_category::air};

void damage_world_cell(uint8_t const column, uint8_t const row, uint16_t const height,
  city_map &cells, resources::geometry_bank const &bank, effect_system &effects, clock_tick const clock, uint8_t &counter) {
  /// 67BF adds a damage stage, carries the resulting high bit into C221 and emits the type's effect at its origin
  auto &cell{cells.at(city_cell_index(column, row))};
  cell.state = static_cast<uint8_t>(cell.state + 32);
  counter = static_cast<uint8_t>(counter + ((cell.state & 128) != 0));
  if(cell.type == 0) return;
  auto const &type{bank.city_types()[cell.type - 1]};
  effects.spawn(building_effect_recipes.at(type.unknown_5),
    {static_cast<uint16_t>(column * 256 + type.column_fraction), static_cast<uint16_t>(row * 256 + type.row_fraction), height}, clock);
}

void impact_projectile_world(city_collision_result const &contact, maths::world_position const impact, object_definition const &definition,
  city_map &cells, resources::geometry_bank const &bank, effect_system &effects, clock_tick const clock, uint16_t const terrain_recipe, uint8_t &counter, uint8_t const damage_mask) {
  /// CDB9 distinguishes marked player targets, hostile bombing and universal category-two breakable components
  if(contact.contact != city_contact::building) {
    effects.spawn(terrain_recipe, impact, clock);
    return;
  }
  auto const &cell{cells[city_cell_index(contact.column, contact.row)]};
  auto const model{bank.city_model_offset(cell.type, cell.state, damage_mask)};
  auto const bytes{bank.model_pool()};
  bool const linked{bytes[model] != std::byte{0} || bytes[model + 1] != std::byte{0}};
  auto const found{std::ranges::find_if(original_object_definitions, [&](auto const &original){
    return &original == &definition;
  })};
  auto const slot{static_cast<size_t>(found - original_object_definitions.begin())};
  if(projectile_damages_building(slot, contact.category, cell.state, linked)) {
    damage_world_cell(contact.column, contact.row, impact.height, cells, bank, effects, clock, counter);
  } else effects.spawn(0x7386, impact, clock);
}

} // anonymous namespace

mission_combat::mission_combat(std::vector<scenario_actor> initial) : actors{std::move(initial)} {
  /// Keep stable native identities and reverse source order within the original air/ground/static category traversal
  std::stable_sort(actors.begin(), actors.end(), [](auto const &a, auto const &b){
    return a.category < b.category;
  });
  outstanding_objectives = static_cast<uint8_t>(std::ranges::count_if(actors, [](auto const &actor){
    return actor.attributes & 1;
  }));
}

std::span<uint8_t const> mission_combat::status_flags(uint8_t const player_flags) noexcept {
  /// C1EF addresses stable object indices, including the player and records removed from active lists
  retained_flags[0] = player_flags;
  for(auto const *group : {&actors, &reserves, &free_actors}) {
    for(auto const &actor : *group) retained_flags[actor.index] = actor.flags;
  }
  return retained_flags;
}

void mission_combat::release_target(uint16_t const token) noexcept {
  /// 7A52 makes missiles targeting a removed record self-guiding and releases its selected lock
  for(auto *pool : {&projectiles, &hostile_projectiles}) {
    for(auto *shot{pool->objects().head()}; shot; shot = shot->next) {
      if(shot->target_token == token) shot->target_token = shot->native_id;
    }
  }
  if(target.token == token) target.clear();
  if(camera_actor && token == native_object_layout::actor(*camera_actor)) camera_actor.reset();
}

void mission_combat::spawn_aircraft(player_flight const &player, city_map const &cells, resources::geometry_bank const &bank, clock_tick const clock, game_duration const frame_step) {
  /// 3E02 visits occupied warehouses before the player and other moving-object callbacks
  if(!player.tunnel) advance_aircraft_spawning(spawning, actors, free_actors, cells, bank, player.pose(), clock, frame_step, random_state);
}

void mission_combat::collide_player(player_flight &player, maths::world_position const &start,
  city_map &cells, resources::geometry_bank const &bank, clock_tick const clock) {
  /// 6F0F scans the player before other collision owners; 6ED4 damages the victim before the player
  player_contact = {};
  if(player.noclip || has_player_flag(player.lifecycle.flags, player_flag::dead)) return;
  auto end{player.pose().position};
  auto const contact{sweep_city(bank, cells, player.world_damage_mask(), start, end, 12, 10)};
  auto *victim{sweep_actor_groups(actors, bank, start, end, 12, actor_collision_groups)};
  if(!victim && contact.contact == city_contact::none) return;
  end.height &= 0xfff8;
  player.pose().position = end;
  if(!victim) {
    player_contact = contact;
    player.apply_city_contact(contact, clock, bank, cells);
    return;
  }
  auto const reaction{hit_actor(*victim, 0x5c, clock, random_state, player.tunnel.has_value())};
  effects.spawn(reaction.effect, reaction.at_actor ? victim->pose.position : end, clock);
  auto &damage{std::visit([](auto &craft)->player_damage_state& {
    return craft.damage;
  }, player.craft)};
  auto const amount{static_cast<uint8_t>(next_random(random_state) | 0x80)};
  apply_player_damage(damage, amount, 0x3c, std::holds_alternative<skimma_flight_state>(player.craft), player.damage_cheat, random_state);
  effects.spawn(0x70f0, end, clock);
  player_hit = true;
  if(reaction.remove) {
    retained_flags[victim->index] = static_cast<uint8_t>(victim->flags | 0x20);
    release_target(native_object_layout::actor(victim->index));
    completed_objectives += victim->attributes & 1;
    adjust_objectives(static_cast<uint8_t>(-(victim->attributes & 1)));
    actors.erase(actors.begin() + (victim - actors.data()));
  }
}

void mission_combat::collide_aircraft(city_map const &cells, resources::geometry_bank const &bank, uint8_t const damage_mask, clock_tick const clock, bool const underground) {
  /// 6DF1 clips each airborne owner against the city and its own list; 6E6D applies strengths 40h and 60h to the pair
  std::array<uint8_t, 256> owners{};
  size_t count{0};
  for(auto const &actor : actors) {
    if(actor.category == actor_category::air) owners[count++] = actor.index;
  }
  auto const find{[&](uint8_t const index){
    return std::ranges::find(actors, index, &scenario_actor::index);
  }};
  auto const remove{[&](uint8_t const index){
    auto const actor{find(index)};
    if(actor == actors.end()) return;
    retained_flags[index] = static_cast<uint8_t>(actor->flags | 0x20);
    release_target(native_object_layout::actor(index));
    completed_objectives += actor->attributes & 1;
    adjust_objectives(static_cast<uint8_t>(-(actor->attributes & 1)));
    actors.erase(actor);
  }};
  for(auto const index : std::span{owners}.first(count)) {
    auto const owner{find(index)};
    if(owner == actors.end() || (owner->flags & 0x10)) continue;
    auto end{owner->pose.position};
    auto const contact{sweep_city(bank, cells, damage_mask, owner->previous_position, end, 12, 10)};
    auto *victim{sweep_actor_groups(actors, bank, owner->previous_position, end, 12, aircraft_collision_group, index)};
    if(victim) {
      end.height &= 0xfff8;
      owner->pose.position = end;
      auto const victim_index{victim->index};
      auto const first{hit_actor(*owner, 0x40, clock, random_state, underground)};
      effects.spawn(first.effect, end, clock);
      auto const second{hit_actor(*victim, 0x60, clock, random_state, underground)};
      effects.spawn(second.effect, second.at_actor ? victim->pose.position : end, clock);
      if(first.remove) remove(index);
      if(second.remove) remove(victim_index);
    } else if(contact.contact != city_contact::none) {
      owner->pose.position = end;
      if(owner->flags & 8) continue;
      effects.spawn(contact.contact == city_contact::building ? 0x716c : 0x7199, end, clock);
      owner->flags |= 0x28;
      owner->parameters.update_entry = object_update::effect_only;
      owner->expiry = static_cast<uint16_t>(clock + 256);
    }
  }
}

void mission_combat::detonate_dual_launch(projectile &shot, clock_tick const clock, bool const underground) {
  /// CBE7 applies separate aircraft, ground and stationary blast passes before retiring both paired projectiles
  for(auto const category : {actor_category::air, actor_category::ground, actor_category::stationary}) {
    for(auto actor{actors.begin()}; actor != actors.end();) {
      auto const strength{actor->category == category ? dual_launch_impact(shot.placement, actor->pose, category == actor_category::air) : std::nullopt};
      if(!strength) {
        ++actor;
        continue;
      }
      auto const reaction{hit_actor(*actor, *strength, clock, random_state, underground)};
      effects.spawn(reaction.effect, actor->pose.position, clock);
      if(!reaction.remove) {
        ++actor;
        continue;
      }
      retained_flags[actor->index] = static_cast<uint8_t>(actor->flags | 0x20);
      release_target(native_object_layout::actor(actor->index));
      completed_objectives += actor->attributes & 1;
      adjust_objectives(static_cast<uint8_t>(-(actor->attributes & 1)));
      actor = actors.erase(actor);
    }
  }
  auto *capsule{projectiles.resolve(shot.target_token)};
  if(!capsule) throw std::logic_error{"Dual Launch detonation lost its paired capsule"};
  effects.spawn(0x71e8, capsule->placement.position, clock);
  for(auto *part : {&shot, capsule}) {
    part->parameters.update_entry = object_update::effect_only;
    part->flags |= 0x28;
    part->deadline = static_cast<uint16_t>(clock + 256);
  }
}

void mission_combat::activate_reserves(actor_category const category, uint8_t const count, object_pose const &player, clock_tick const clock) {
  /// C37A adds each admitted record's objective bit without the signed clamp used by removal
  auto const counted{[&]{
    return std::ranges::count_if(actors, [](auto const &actor){
    return actor.attributes & 1;
  });
  }};
  auto const before{counted()};
  activate_scenario_reserves(actors, reserves, category, count, player, clock);
  outstanding_objectives = static_cast<uint8_t>(outstanding_objectives + counted() - before);
}

void mission_combat::adjust_objectives(uint8_t const operand) noexcept {
  /// Script 33 and object removal share C16E's mutable outstanding counter
  outstanding_objectives = adjust_objective_counter(outstanding_objectives, operand);
}

void mission_combat::update_difficulty(campaign_clock const clock) noexcept {
  /// 3DB1–3DCB increase the scenario's firing pressure after four clock wraps, saturating after eight
  auto const wraps{static_cast<uint8_t>(clock >> 16)};
  if(wraps < 4) return;
  auto const pressure{wraps >= 8 ? 255u : ((clock >> 8) - 1024) >> 2};
  difficulty = std::max(difficulty, static_cast<uint8_t>(pressure));
}

unsigned int mission_combat::remaining_objectives() const noexcept {
  /// C16F is independently mutable: withdrawal scripts can clear obligations while aircraft remain
  return outstanding_objectives;
}

void mission_combat::fire_skimma_primary(player_flight const &player, city_map const &cells, resources::geometry_bank const &bank,
  clock_tick const clock, game_duration const frame_step, bool const pressed) {
  /// C950 updates recoil every frame; CD6C traces the short primary ray and applies strength 32h only to aircraft
  auto const recoil{calculate_skimma_recoil(skimma.recoil, frame_step)};
  skimma.recoil = recoil.next;
  skimma.aim_offset = recoil.aim_offset;
  if(!pressed || has_player_flag(player.lifecycle.flags, player_flag::dead)) return;
  auto end{skimma_gun_endpoint(player.pose(), recoil.shot_offset, random_state)};
  sweep_city(bank, cells, player.world_damage_mask(), player.pose().position, end, 0, 10);
  auto *victim{sweep_actor_groups(actors, bank, player.pose().position, end, 0, aircraft_collision_group)};
  auto impact{end};
  impact.height &= 0xfff8;
  bool const hit{victim != nullptr};
  if(victim) {
    auto const reaction{hit_actor(*victim, 0x32, clock, random_state)};
    effects.spawn(reaction.effect, reaction.at_actor ? victim->pose.position : impact, clock);
    if(reaction.remove) {
      retained_flags[victim->index] = static_cast<uint8_t>(victim->flags | 0x20);
      release_target(native_object_layout::actor(victim->index));
      completed_objectives += victim->attributes & 1;
      adjust_objectives(static_cast<uint8_t>(-(victim->attributes & 1)));
      actors.erase(actors.begin() + (victim - actors.data()));
    }
  }
  effects.gun_impact(impact, hit, clock);
  skimma.recoil = kick_skimma_recoil(skimma.recoil, static_cast<uint8_t>(next_random(random_state)));
  player_fired = true;
}

void mission_combat::advance(player_flight &player, city_map &cells, resources::geometry_bank const &bank,
  combat_timing const timing, combat_input const input, combat_scenario const scenario,
  std::optional<maths::world_position> const player_start) {
  /// Follow actor scripts and motion, player firing, projectile movement and collision/removal phases
  auto const [elapsed_ticks, frame_step, changes]{timing};
  auto const [trigger_pressed, secondary_pressed, secondary_held, trigger_released]{input};
  auto const [routes, script_multiplier, network]{scenario};
  auto const clock{static_cast<uint16_t>(elapsed_ticks)};
  effects.advance(clock, frame_step);
  threat_errors.fill(64);
  player_fired = false;
  if(player.lifecycle.crashing) weapon_charge = 0;
  player_hit = false;
  player_contact = {};
  auto *caero{std::get_if<caero_flight_state>(&player.craft)};
  auto &damage{std::visit([](auto &craft)->player_damage_state& {
    return craft.damage;
  }, player.craft)};
  auto const player_definition{player.definition_slot()};
  auto const damage_mask{player.world_damage_mask()};
  auto const player_extent{bank.header_at(bank.special_models()[player_definition]).extent};
  std::erase_if(actors, [&](auto &actor){
    if(!advance_object_deadline(actor.flags, actor.expiry, actor.fade, actor.pose.position.height, clock)) return false;
    release_target(native_object_layout::actor(actor.index));
    completed_objectives += actor.attributes & 1;
    adjust_objectives(static_cast<uint8_t>(-(actor.attributes & 1)));
    auto const lifetime{static_cast<uint8_t>(actor.attributes & 0xfe)};
    actor.flags |= 0x20;
    retained_flags[actor.index] = actor.flags;
    if(lifetime && actor.category == actor_category::air) {
      actor.attributes = lifetime < 0xfe ? static_cast<uint8_t>(lifetime - 2) : lifetime;
      free_actors.insert(free_actors.begin(), actor);
    }
    return true;
  });
  for(auto const category : {actor_category::air, actor_category::ground, actor_category::stationary}) {
    // new heads in this category wait until the next pass; later categories see admissions immediately
    std::array<uint8_t, 256> update_order{};
    size_t count{0};
    for(auto const &actor : actors) {
      if(actor.category == category) update_order[count++] = actor.index;
    }
    for(auto const index : std::span{update_order}.first(count)) {
      auto const find_actor{[&]{
        return std::ranges::find(actors, index, &scenario_actor::index);
      }};
      auto const callback{find_actor()->parameters.update_entry};
      if((callback == object_update::surface_actor || callback == object_update::tunnel_actor) && !find_actor()->script.stopped) {
        // script callbacks may insert into actors, so retain the executing record independently of vector storage
        auto actor{*find_actor()};
        mission_context context{
          .program{routes},
          .object_flags{status_flags(player.lifecycle.flags)},
          .cells{cells},
          .clock{elapsed_ticks},
          .time_multiplier{script_multiplier},
          .object_counter{static_cast<uint8_t>(completed_objectives)},
          .current_cell{static_cast<uint16_t>((actor.pose.position.column >> 8) | (actor.pose.position.row & 0xff00))},
          .set_target{[&](uint16_t const target, bool const flag_02){
            actor.target_token = target;
            actor.flags = static_cast<uint8_t>((actor.flags & 0xfd) | (flag_02 ? 2 : 0));
          }},
          .retire_distant_actor{[&]{
            return darker::game::retire_distant_actor(actor, player.pose(), clock);
          }}
        };
        context.adjust_objectives = [&](uint8_t const operand){
          adjust_objectives(operand);
          return remaining_objectives() == 0;
        };
        context.activate_reserves = [&](uint8_t const opcode, uint8_t const count){
          activate_reserves(static_cast<actor_category>(opcode - 9), count, player.pose(), clock);
          return remaining_objectives() == 0;
        };
        context.set_tunnel_oscillation = [&](uint8_t const phase){
          if(!actor.tunnel) throw std::logic_error{"Tunnel direction command requires an underground actor"};
          actor.tunnel->oscillation = phase;
        };
        context.register_owner = [&]{
          return std::exchange(script_owner, native_object_layout::actor(actor.index));
        };
        advance_mission_script(actor.script, context);
        *find_actor() = std::move(actor);
      }
      auto &actor{*find_actor()};
      if(actor.parameters.update_entry == object_update::effect_only) continue;
      if(actor.parameters.update_entry == object_update::ground_vehicle) {
        if(!actor.route) throw std::logic_error{"Ground callback has no vehicle route"};
        actor.previous_position = actor.pose.position;
        auto const event{advance_vehicle_route(*actor.route, actor.pose, actor.flags, routes, clock, bank.header_at(actor.parameters.model_token).height, random_state)};
        if(event.firing_direction) fire_vehicle_missile(hostile_projectiles, actor, player.pose(), cells, bank.city_types(),
          *event.firing_direction, clock, difficulty, bank.special_models()[18]);
        if(event.damage_cell) {
          damage_world_cell(static_cast<uint8_t>(*event.damage_cell), static_cast<uint8_t>(*event.damage_cell >> 8), 0xe0, cells, bank, effects, clock, world_damage_counter);
        }
        if(event.deadline) actor.expiry = *event.deadline;
        if(event.effect) {
          auto const &effect{*event.effect};
          if(effect.recipe) effects.spawn(effect.recipe, effect.position, clock);
          else effects.spark(effect.position, effect.phase, effect.sound_level, clock);
        }
        continue;
      }
      if(actor.parameters.update_entry == object_update::inactive) continue;
      if(callback == object_update::falling_aircraft) advance_falling_aircraft(actor, frame_step);
      else if(callback == object_update::departing_aircraft) advance_aircraft_departure(actor, clock, frame_step);
      else if(callback == object_update::tunnel_actor) {
        if(!network) throw std::logic_error{"Underground actor update requires its route network"};
        advance_tunnel_actor(actor, player.pose(), actors, cells, *network, frame_step);
      } else advance_surface_actor(actor, player.pose(), actors, cells, bank, damage_mask, frame_step,
        [&](scenario_actor &source, actor_course const course, uint8_t const distance){
          auto const shot{fire_skimma_gun(source, player.pose(), player.lifecycle.flags, player_extent, course, distance, clock, changes, random_state, player_definition <= 25 ? 0x30 : 0x20)};
          if(shot) effects.gun_impact(shot->end, shot->hit, clock);
          if(shot && shot->hit) {
            apply_player_damage(damage, 0x15, 3, !caero, player.damage_cheat, random_state);
            player_hit = true;
          }
          if(source.selected_target == native_object_layout::player || target_reference{source.selected_target}.is_ground_encoded()) {
            if(auto const slot{aircraft_projectile_definition(source, player.lifecycle.flags, course, distance, clock, difficulty, building_attacks)}) {
              // CAF0 records the attempt time even if the hostile pool is exhausted
              source.last_shot = clock;
              auto const &definition{original_object_definitions[*slot]};
              launch_emitter const launcher{
                .position{source.pose.position},
                .fractions{source.pose.fractions},
                .angles{source.pose.angles},
                .speed{source.pose.speed},
                .side_flags{source.flags},
                .definition_strength{source.parameters.definition->impact_strength}
              };
              hostile_projectiles.launch({
                .definition{definition},
                .emitter{launcher},
                .model_token{bank.special_models()[*slot]},
                .clock{clock},
                .lifetime{static_cast<uint16_t>(definition.role_data.projectile().lifetime * 256)},
                .target_token{source.selected_target}
              });
            }
          }
        }, [&](scenario_actor &source){
          drop_aircraft_bomb(hostile_projectiles, source, building_attacks, clock, bank.special_models()[14]);
        }, &threat_errors);
      if(auto const severity{damage_trail_severity(actor.awareness.cooldown, actor.flags, changes)}) {
        effects.trail(actor.previous_position, *severity, random_state, clock);
      }
    }
  }
  auto const &pose{player.pose()};
  launch_emitter const emitter{
    .position{pose.position},
    .fractions{pose.fractions},
    .angles{pose.angles},
    .speed{pose.speed},
    .side_flags{player.lifecycle.flags},
    .definition_strength{original_object_definitions[player_definition].impact_strength}
  };
  weapon_ready = false;
  if(!caero) {
    skimma.update_status(clock, std::bit_cast<int16_t>(target.token), projectiles.objects().free_head() ? 1 : 0, player.upgraded);
    fire_skimma_primary(player, cells, bank, clock, frame_step, trigger_pressed);
  }
  if(caero && uses_primary_trigger(static_cast<caero_weapon>(primary_weapon))) {
    auto const result{fire_caero_weapon(projectiles, caero->energy, weapon_charge, {
      .emitter{emitter},
      .selection{primary_weapon},
      .player_flags{player.lifecycle.flags},
      .pressed{trigger_pressed},
      .model{bank.special_models()[caero_definition_slot(static_cast<caero_weapon>(primary_weapon))]},
      .clock{clock},
      .frame_step{frame_step},
      .underground{player.tunnel.has_value()},
      .released{trigger_released}
    })};
    weapon_ready = result.ready;
    if(result.next_selection) primary_weapon = result.next_selection;
    player_fired = result.shot != nullptr;
    if(result.shot && missile_camera_enabled) camera_projectile = result.shot;
  }
  secondary_ready = false;
  if(caero && uses_secondary_trigger(static_cast<caero_weapon>(secondary_weapon))) {
    auto const result{fire_caero_weapon(projectiles, caero->energy, weapon_charge, {
      .emitter{emitter},
      .selection{secondary_weapon},
      .player_flags{player.lifecycle.flags},
      .pressed{secondary_pressed},
      .held{secondary_held},
      .model{bank.special_models()[caero_definition_slot(static_cast<caero_weapon>(secondary_weapon))]},
      .clock{clock},
      .frame_step{frame_step},
      .target{target.token},
      .underground{player.tunnel.has_value()}
    })};
    secondary_ready = result.ready;
    if(result.next_selection) secondary_weapon = result.next_selection;
    player_fired |= result.shot != nullptr;
    if(result.shot && missile_camera_enabled) camera_projectile = result.shot;
  }
  if(!caero) {
    auto &slot{skimma.slots.at(skimma.selection)};
    auto *shot{fire_skimma_weapon(projectiles, slot, {
      .emitter{emitter},
      .weapon{skimma.selection},
      .player_flags{player.lifecycle.flags},
      .pressed{secondary_pressed},
      .model{bank.special_models()[object_catalogue::skimma_definition(skimma.selection)]},
      .clock{clock},
      .target{target.token}
    })};
    secondary_ready = slot.flags == 3;
    player_fired |= shot != nullptr;
    if(shot && missile_camera_enabled) camera_projectile = shot;
  }
  auto const resolve_target{[&](projectile &shot)->projectile_target {
    if(shot.parameters.update_entry == object_update::mimic) return &player.pose();
    if(shot.parameters.update_entry != object_update::homing_projectile && shot.parameters.update_entry != object_update::chargeable && shot.parameters.update_entry != object_update::dual_launch) return {};
    if(shot.target_token == native_object_layout::player) return &player.pose();
    if(target_reference{shot.target_token}.is_ground_encoded()) return resolve_map_guidance(shot.target_token, cells, bank, damage_mask);
    if(shot.target_token == shot.native_id) return &shot.placement;
    auto const actor{std::ranges::find_if(actors, [&](auto const &candidate){
      return native_object_layout::actor(candidate.index) == shot.target_token;
    })};
    if(actor != actors.end()) return &actor->pose;
    if(auto const *other{projectiles.resolve(shot.target_token)}) return &other->placement;
    if(auto const *other{hostile_projectiles.resolve(shot.target_token)}) return &other->placement;
    throw std::logic_error{"Guided projectile target has no active object record"};
  }};
  for(auto *shot{projectiles.objects().head()}; shot;) {
    auto const destination{resolve_target(*shot)};
    auto const *paired{std::get_if<object_pose const*>(&destination)};
    auto const separation{shot->parameters.update_entry == object_update::dual_launch && paired && *paired && *paired != &shot->placement
      ? std::optional<uint16_t>{dual_launch_separation(shot->placement, **paired)} : std::nullopt};
    auto const result{(shot->flags & 8) ? (update_projectile_deadline(*shot, clock) ? projectile_update_result::expired : projectile_update_result::advanced)
      : update_projectile(*shot, clock, frame_step, destination)};
    if(result == projectile_update_result::expired) {
      release_target(shot->native_id);
      if(camera_projectile == shot) camera_projectile = nullptr;
      shot = projectiles.recycle(*shot);
      continue;
    }
    if(result == projectile_update_result::detonated) detonate_dual_launch(*shot, clock, player.tunnel.has_value());
    else if(separation) skimma.dual_launch_pitch = static_cast<uint16_t>((0x80c - std::min<uint16_t>(*separation, 0xcd)) >> 2);
    shot = shot->next;
  }
  for(auto *shot{hostile_projectiles.objects().head()}; shot;) {

    bool const expired{(shot->flags & 8) ? update_projectile_deadline(*shot, clock)
      : update_projectile(*shot, clock, frame_step, resolve_target(*shot)) == projectile_update_result::expired};
    if(expired) {
      release_target(shot->native_id);
      shot = hostile_projectiles.recycle(*shot);
      continue;
    }
    shot = shot->next;
  }
  bool const reloading{std::bit_cast<int16_t>(static_cast<uint16_t>(clock - skimma.ring.reload_deadline)) < 0};
  if(caero ? !secondary_weapon : (!(skimma.slots.at(skimma.selection).flags & 1) || reloading)) target.clear();
  else {
    if(target.token == weapon_target::no_target) target.token = acquire_caero_target(player.pose(), actors, cells, bank, damage_mask);
    if(target.token != 0xffff) {
      if(target_reference{target.token}.is_object_encoded()) {
        auto const found{std::ranges::find_if(actors, [&](auto const &actor){
          return native_object_layout::actor(actor.index) == target.token;
        })};
        if(found == actors.end()) target.clear();
        else {
          auto const extent{bank.header_at(found->parameters.model_token).extent};
          if(caero) target.project_caero(pose.position, found->pose.position, extent, targeting_basis, secondary_weapon);
          else target.project_skimma(pose.position, found->pose.position, extent, targeting_basis, skimma.selection, true, false, true);
        }
      } else {
        auto const aim{resolve_map_guidance(target.token, cells, bank, damage_mask)};
        auto const cell{cells[packed_cell_reference{target.token}.index()]};
        maths::world_position const position{
          .column{aim.position.column},
          .row{aim.position.row},
          .height{aim.height}
        };
        if(caero) target.project_caero(pose.position, position, aim.height_extent, targeting_basis, secondary_weapon, cell.type, cell.state);
        else {
          auto const model{bank.city_model_offset(cell.type, cell.state, damage_mask)};
          auto const bytes{bank.model_pool()};
          bool const linked{bytes[model] != std::byte{0} || bytes[model + 1] != std::byte{0}};
          target.project_skimma(pose.position, position, aim.height_extent, targeting_basis, skimma.selection, true, false, linked);
        }
      }
    }
  }
  if(has_player_flag(player.lifecycle.flags, player_flag::ground_protection)) target.clear();
  if(!caero) skimma.ring.target_spread = target.spread;
  if(player_start) collide_player(player, *player_start, cells, bank, clock);
  collide_aircraft(cells, bank, damage_mask, clock, player.tunnel.has_value());
  for(auto *shot{projectiles.objects().head()}; shot; shot = shot->next) {
    if(shot->flags & 8) continue;
    auto end{shot->placement.position};
    auto const contact{sweep_city(bank, cells, damage_mask, shot->previous_position, end, 2, 10)};
    auto *victim{sweep_actor_groups(actors, bank, shot->previous_position, end, 2, actor_collision_groups)};
    auto impact{end};
    impact.height &= 0xfff8;
    if(!victim && contact.contact == city_contact::none) continue;
    shot->placement.position = impact;
    auto const strength{shot->parameters.definition == &original_object_definitions[8]
      ? chargeable_impact_strength(shot->deadline, clock) : std::optional<impact_strength>{shot->parameters.definition == &original_object_definitions[0]
        ? pinner_direct_strength(player.tunnel.has_value()) : shot->parameters.definition->impact_strength}};
    if(victim && strength) {
      auto const strength_input{shot->parameters.definition == &original_object_definitions[7]
        ? caero_weapon_strength(cells, impact, native_object_layout::actor(victim->index)) : *strength};
      auto const reaction{hit_actor(*victim, strength_input, clock, random_state, player.tunnel.has_value())};
      effects.spawn(reaction.effect, reaction.at_actor ? victim->pose.position : impact, clock);
      if(reaction.remove) {
        retained_flags[victim->index] = static_cast<uint8_t>(victim->flags | 0x20);
        release_target(native_object_layout::actor(victim->index));
        completed_objectives += victim->attributes & 1;
        adjust_objectives(static_cast<uint8_t>(-(victim->attributes & 1)));
        actors.erase(actors.begin() + (victim - actors.data()));
      }
    }
    else if(!victim && contact.contact == city_contact::building
      && (shot->parameters.definition == &original_object_definitions[3] || shot->parameters.definition == &original_object_definitions[4])) {
      auto const &cell{cells[city_cell_index(contact.column, contact.row)]};
      auto const result{diffuser.hit(shot->parameters.definition == &original_object_definitions[4], contact.category, cell.state,
        packed_cell_reference::from_coordinates(contact.column, contact.row).value, clock)};
      if(result == diffuser_impact::destroyed) damage_world_cell(contact.column, contact.row, impact.height, cells, bank, effects, clock, world_damage_counter);
      else if(result == diffuser_impact::gas) {
        auto const &type{bank.city_types()[cell.type - 1]};
        effects.spawn(0x75d4, {static_cast<uint16_t>(contact.column * 256 + type.column_fraction),
          static_cast<uint16_t>(contact.row * 256 + type.row_fraction), impact.height}, clock);
      } else effects.spawn(0x75a3, impact, clock);
    }
    else if(!victim) impact_projectile_world(contact, impact, *shot->parameters.definition, cells, bank, effects, clock, player.tunnel ? 0x7386 : 0x721c, world_damage_counter, damage_mask);
    shot->flags |= 0x28;
    shot->parameters.update_entry = object_update::effect_only;
    shot->deadline = static_cast<uint16_t>(clock + 256);
  }
  for(auto *shot{hostile_projectiles.objects().head()}; shot; shot = shot->next) {
    if(shot->flags & 8) continue;
    auto end{shot->placement.position};
    auto const contact{sweep_city(bank, cells, damage_mask, shot->previous_position, end, 2, 10)};
    auto candidate{end};
    bool const hit{!has_player_flag(player.lifecycle.flags, player_flag::dead) && sweep_aircraft(player.pose(), player_extent, 2, shot->previous_position, candidate)};
    if(!hit && contact.contact == city_contact::none) continue;
    end.height &= 0xfff8;
    shot->placement.position = end;
    if(hit) {
      // 6E95 halves definition strength and derives the angular kick from that amount
      uint8_t const amount{static_cast<uint8_t>(shot->parameters.definition->impact_strength >> 1)};
      apply_player_damage(damage, amount, static_cast<uint8_t>((amount >> 1) - 7), !caero, player.damage_cheat, random_state);
      effects.spawn(0x70c3, end, clock);
      player_hit = true;
    } else {
      impact_projectile_world(contact, end, *shot->parameters.definition, cells, bank, effects, clock, 0x7199, world_damage_counter, damage_mask);
    }
    shot->flags |= 0x28;
    shot->parameters.update_entry = object_update::effect_only;
    shot->deadline = static_cast<uint16_t>(clock + 256);
  }
  if(player_damage_is_lethal(damage) && start_player_crash(player.pose(), player.lifecycle, clock)) player.engine_flags = 0;
}

} // namespace darker::game
