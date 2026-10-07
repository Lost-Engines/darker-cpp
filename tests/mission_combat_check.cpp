#include "mission_combat_check.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <utility>
#include "game/actor_activation.h"
#include "game/hangar.h"
#include "game/mission_combat.h"
#include "game/object_definitions.h"
#include "graphics/city_scene.h"
#include "graphics/formatted_text.h"
#include "presentation/player.h"
#include "resources/archive_set.h"

void check_mission_combat(darker::resources::archive_set const &archives) {
  /// Drive real campaign projectiles through real aircraft hulls, then observe removal and the original completion script
  darker::resources::geometry_bank const bank{archives.load({.archive{0}, .slot{30}})};
  auto cells{darker::game::make_city_map(archives.load({.archive{0}, .slot{68}}), true)};
  std::array<uint8_t, 256> limits{};
  for(size_t i{0}; i < bank.city_types().size(); ++i) limits[i + 1] = bank.city_types()[i].variant_limit;
  darker::game::assign_city_variants(cells, limits);
  darker::resources::scenario_resource const scenario{archives.load({.archive{4}, .slot{0}})};
  for(size_t mission{0}; mission < 8; ++mission) {
    auto const &record{scenario.records()[mission]};
    darker::game::mission_combat combat{darker::game::make_scenario_group(record.groups[0], bank, 1, 0, record.shared.offset)};
    combat.reserves = darker::game::make_scenario_group(record.groups[1],bank,static_cast<uint8_t>(1 + record.groups[0].objects.size()),0,record.shared.offset);
    darker::game::player_flight player;
    auto &caero{std::get<darker::game::caero_flight_state>(player.craft)};
    caero.flying = true;
    caero.energy.reserve = 0xcfff;
    combat.primary_weapon = mission < 4 ? 1 : 2;
    combat.difficulty = static_cast<uint8_t>((mission + 1)*2);
    darker::resources::font_resource const fonts{archives.load({.archive{0}, .slot{29}})};
    auto const text{scenario.language(mission, darker::resources::scenario_language::english)};
    darker::presentation::player briefing{archives,fonts,scenario,mission};
    do { briefing.advance(4000); } while(briefing.continue_page());
    auto const cursor{briefing.consumed_text()};
    darker::game::mission_context context{.program{scenario.bytes(record.shared)}, .text{text}, .cells{cells}, .time_multiplier{record.time_multiplier}, .text_cursor{cursor}};
    darker::game::mission_script script{.continuation{*record.player_program - record.shared.offset}};
    context.activate_reserves = [&](uint8_t const opcode, uint8_t const count){
      darker::game::activate_scenario_reserves(combat.actors,combat.reserves,static_cast<darker::game::actor_category>(opcode - 9),count,player.pose(),static_cast<uint16_t>(context.clock));
      return combat.remaining_objectives() == 0;
    };
    unsigned int shots{0};
    bool message{false};
    bool saw_burst{false}, saw_trail{false};
    for(uint32_t clock{8}; clock < 300000; clock += 8) {
      auto const target{std::ranges::find_if(combat.actors, [](auto const &actor){ return (actor.attributes & 1) && !(actor.flags & 0x20); })};
      bool const fire{target != combat.actors.end() && clock % 128 == 0};
      if(target != combat.actors.end()) {
        player.pose().position = target->pose.position;
        player.pose().position[1] += 200;
        player.pose().position[2] += 92;
        player.pose().angles = {};
        player.pose().speed = 496;
      }
      combat.advance(player, cells, bank, clock, 8, static_cast<uint16_t>(clock ^ (clock - 8)), fire, scenario.bytes(record.shared));
      if(combat.player_fired) ++shots;
      saw_burst |= !combat.effects.emitters.empty();
      saw_trail |= !combat.effects.trails.empty();
      // Supply controlled beacon power while isolating aim/collision/completion from navigation.
      darker::game::charge_caero_energy(caero.energy, 13056, 1, 1028, false);
      context.clock = clock;
      context.objectives_complete = combat.remaining_objectives() == 0;
      context.messages.clear();
      darker::game::advance_mission_script(script, context);
      for(auto const &event : context.messages) {
        if(mission < 3 && !context.objectives_complete) throw std::runtime_error{"Return message preceded objective completion"};
        std::string const actual{reinterpret_cast<char const *>(text.data() + event.offset), event.length};
        if(mission < 3 && actual != std::array{"Well done- you can return to base.","Mission accomplished. Return to base.","Good job, Tolly. Return to base."}[mission]) throw std::runtime_error{"Incorrect first-mission return message"};
        constexpr std::array final_messages{"Well done- you can return to base.","Mission accomplished. Return to base.",
          "Good job, Tolly. Return to base.","all targets are clear.","Mission complete- come back to base.",
          "Well done- you can return to base.","Return to Hemmersan.","Mission complete- come back to base."};
        message |= actual == final_messages[mission];
      }
      if(message && script.stopped) break;
    }
    if(!message || !script.stopped || combat.completed_objectives != std::array{2u,2u,3u,5u,3u,5u,8u,8u}[mission] || combat.remaining_objectives() != 0 || player.lifecycle.crashing) {
      throw std::runtime_error{"Campaign controlled combat did not complete: mission=" + std::to_string(mission + 1) + ", shots=" + std::to_string(shots)
        + ", removed=" + std::to_string(combat.completed_objectives) + ", remaining=" + std::to_string(combat.remaining_objectives()) + ", reserves=" + std::to_string(combat.reserves.size())
        + ", stopped=" + std::to_string(script.stopped) + ", message=" + std::to_string(message) + ", crashing=" + std::to_string(player.lifecycle.crashing)
        + ", reserve=" + std::to_string(caero.energy.reserve)};
    }
    if(!saw_burst || !saw_trail) throw std::runtime_error{"Combat omitted hit bursts or damage trails"};
    player.pose().position = {12672, 28380, 500};
    player.pose().angles = {0x8000, 0, 0};
    caero.damage.rotation = {};
    darker::game::hangar_state hangar;
    if(!darker::game::begin_hangar_return(player, cells, hangar, combat.remaining_objectives() == 0)) throw std::runtime_error{"Completed mission refused HQ return"};
    for(unsigned int frame{0}; frame < 2000 && hangar.returning != darker::game::hangar_return_phase::complete; ++frame) {
      context.clock += 8;
      darker::game::advance_hangar_return(player, hangar, 8, static_cast<uint16_t>(context.clock));
      darker::game::advance_hangar_departure(player, cells, hangar, 8);
    }
    if(hangar.returning != darker::game::hangar_return_phase::complete) throw std::runtime_error{"First mission did not finish docking"};
    std::cout << "Mission " << mission + 1 << " controlled combat: " << shots << " shots, " << combat.completed_objectives << " objectives removed, return message and completed HQ docking verified." << std::endl;
  }
  // Mission two's aircraft are distant from HQ: word projection alone used to show phantom nearby ships.
  {
    auto const actors{darker::game::make_scenario_group(scenario.records()[1].groups[0],bank,1,0,scenario.records()[1].shared.offset)};
    std::vector<darker::graphics::scene_object> objects;
    for(auto const &actor : actors) objects.push_back({.model_offset{actor.parameters.model_token}, .pose{actor.pose}});
    darker::graphics::city_renderer renderer;
    darker::graphics::distance_shading const lighting;
    darker::game::city_map const empty_city{};
    for(unsigned int heading{0}; heading < 65536; heading += 4096) {
      framework::render::indexed_cockpit_framebuffer frame{};
      darker::graphics::city_view const view{.column{12672}, .row{28928}, .altitude{500}, .angles{.heading{static_cast<uint16_t>(heading)}}};
      if(renderer.draw(frame,bank,empty_city,view,0x20,lighting,{},objects) != 0
        || std::ranges::any_of(frame.pixels,[](auto pixel){ return pixel != 0; })) {
        throw std::runtime_error{"Distant mission-two aircraft aliased into the hangar view"};
      }
    }
    objects.front().pose.position = {12672, 28672, 500};
    framework::render::indexed_cockpit_framebuffer frame{};
    if(renderer.draw(frame,bank,empty_city,{.column{12672}, .row{28928}, .altitude{500}},0x20,lighting,{},objects) == 0) {
      throw std::runtime_error{"Nearby mission-two aircraft disappeared with the distant-object rejection"};
    }
  }
  // Exercise mission two's missile branch through the real pool, homing callback, collision and damage response.
  auto missile_actor{darker::game::make_scenario_group(scenario.records()[1].groups[0],bank,1,0,scenario.records()[1].shared.offset).front()};
  missile_actor.pose = {.position{10000,10700,10000},.angles{},.speed{500}};
  missile_actor.awareness.level = 0xff00;
  missile_actor.selected_target = missile_actor.target_token = 0xd986;
  darker::game::mission_combat missiles{{missile_actor}};
  missiles.difficulty = 4;
  darker::game::city_map empty_city{};
  darker::game::player_flight target;
  target.pose().position = {10000,10000,10000};
  missiles.advance(target,empty_city,bank,8192,8,0,false);
  auto *missile{missiles.hostile_projectiles.objects().head};
  if(!missile || missile->parameters.definition != &darker::game::original_object_definitions[10]
    || missiles.projectiles.objects().head || missiles.actors.front().last_shot != 8192) {
    throw std::runtime_error{"Mission-two aircraft did not launch its separate homing missile"};
  }
  missiles.actors.clear();
  bool missile_hit{false};
  for(uint16_t clock{8200}; clock < 11000 && !missile_hit; clock += 8) {
    missiles.advance(target,empty_city,bank,clock,8,0,false);
    missile_hit = missiles.player_hit;
  }
  if(!missile_hit || std::get<darker::game::caero_flight_state>(target.craft).damage.damage != 45 || missiles.effects.emitters.empty()) {
    throw std::runtime_error{"Hostile homing missile did not reach its target with the native half-strength hit"};
  }
  std::cout << "Mission-two missile launched, homed, hit the player for 45 damage and emitted its original impact effect." << std::endl;
  darker::game::mission_combat guided{{}};
  darker::game::player_flight gunner;
  gunner.pose().position = {10000,10000,10000};
  std::get<darker::game::caero_flight_state>(gunner.craft).energy.reserve = 0xcfff;
  guided.primary_weapon = 2;
  guided.missile_camera_enabled = true;
  guided.advance(gunner,empty_city,bank,100,8,0,true);
  if(!guided.camera_projectile || guided.camera_projectile->parameters.definition != &darker::game::original_object_definitions[1]) {
    throw std::runtime_error{"Mimic launch did not register the missile camera"};
  }
  guided.primary_weapon = 0;
  for(uint16_t clock{108}; clock < 2200; clock += 8) guided.advance(gunner,empty_city,bank,clock,8,0,false);
  if(guided.camera_projectile) throw std::runtime_error{"Expired projectile retained the missile camera"};
  // Follow the actual fourth-mission flatbed, with the player and aircraft excluded from this route check.
  auto const &record{scenario.records()[3]};
  auto group{darker::game::make_scenario_group(record.groups[0],bank,1,0,record.shared.offset)};
  std::erase_if(group,[](auto const &actor){ return actor.definition_slot != 31; });
  if(group.size() != 1 || !group.front().route) throw std::runtime_error{"Fourth mission is missing its flatbed route"};
  {
    darker::game::mission_combat target_convoy{group};
    darker::game::player_flight attacker;
    auto &energy{std::get<darker::game::caero_flight_state>(attacker.craft).energy};
    target_convoy.primary_weapon = 1;
    uint16_t hit_clock{0};
    for(uint16_t clock{8}; clock < 2048; clock += 8) {
      attacker.pose().position = target_convoy.actors.front().pose.position;
      attacker.pose().position[1] += 200;
      // Aim above the ground-level origin, inside the truck's native extent cube.
      attacker.pose().position[2] += static_cast<uint16_t>(92 + bank.header_at(target_convoy.actors.front().parameters.model_token).extent);
      attacker.pose().angles = {};
      attacker.pose().speed = 496;
      energy.reserve = 0xcfff;
      target_convoy.advance(attacker,cells,bank,clock,8,0,clock == 128,scenario.bytes(record.shared));
      if(target_convoy.actors.front().flags & 0x20) { hit_clock = clock; break; }
    }
    if(!hit_clock || target_convoy.effects.emitters.empty()
      || target_convoy.actors.front().expiry != static_cast<uint16_t>(hit_clock + 256)
      || target_convoy.actors.front().parameters.update_entry != 0x8f3b) {
      throw std::runtime_error{"Shooting the fourth-mission truck did not schedule its native destruction effect and removal"};
    }
    for(uint16_t elapsed{8}; elapsed <= 264; elapsed += 8) {
      target_convoy.advance(attacker,cells,bank,static_cast<uint16_t>(hit_clock + elapsed),8,0,false,scenario.bytes(record.shared));
      if((elapsed <= 256) != !target_convoy.actors.empty()) throw std::runtime_error{"Shot truck disappeared at the wrong deadline"};
    }
    if(target_convoy.completed_objectives != 0) throw std::runtime_error{"Shooting the uncounted truck credited a mission objective"};
    std::cout << "Mission-four truck: Pinner impact, destruction effect, 256-tick deadline and uncounted removal verified." << std::endl;
  }
  darker::game::mission_combat convoy{std::move(group)};
  darker::game::player_flight observer;
  auto const before{cells};
  unsigned int calls{0};
  for(unsigned int tick{0}; tick < 200000 && !convoy.actors.empty(); tick += 50) {
    convoy.advance(observer,cells,bank,static_cast<uint16_t>(tick),50,0,false,scenario.bytes(record.shared));
    ++calls;
  }
  if(!convoy.actors.empty() || calls != 3114 || convoy.completed_objectives != 0) {
    throw std::runtime_error{"Fourth-mission flatbed removal differs from native route timing"};
  }
  for(size_t i{0}; i < cells.size(); ++i) {
    if(cells[i].type != before[i].type || cells[i].state != before[i].state) throw std::runtime_error{"Flatbed movement changed the city"};
  }
  std::cout << "Fourth-mission flatbed traversed its original route and removed itself after 155,650 ticks at 50-tick sampling." << std::endl;

}
