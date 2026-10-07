#include "mission_combat_check.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>
#include "game/actor_activation.h"
#include "game/beacon_changes.h"
#include "game/hangar.h"
#include "game/mission_combat.h"
#include "game/object_definitions.h"
#include "game/tunnel_flight.h"
#include "game/tunnel_navigation.h"
#include "game/tunnel_portal.h"
#include "graphics/city_scene.h"
#include "graphics/formatted_text.h"
#include "presentation/player.h"
#include "reference/tunnel_actor_samples.h"
#include "reference/tunnel_connection_samples.h"
#include "reference/tunnel_flight_samples.h"
#include "reference/tunnel_navigation_samples.h"
#include "reference/tunnel_placement_samples.h"
#include "reference/tunnel_portal_samples.h"
#include "reference/tunnel_reacquisition_samples.h"
#include "reference/tunnel_return_samples.h"
#include "reference/tunnel_scripted_actor_samples.h"
#include "reference/tunnel_trace_samples.h"
#include "resources/archive_set.h"
#include "resources/campaign.h"

void check_mission_combat(darker::resources::archive_set const &archives) {
  /// Drive real campaign projectiles through real aircraft hulls, then observe removal and the original completion script
  darker::resources::geometry_bank const bank{archives.load({.archive{0}, .slot{30}})};
  auto cells{darker::game::make_city_map(archives.load({.archive{0}, .slot{68}}), true)};
  std::array<uint8_t, 256> limits{};
  for(size_t i{0}; i < bank.city_types().size(); ++i) limits[i + 1] = bank.city_types()[i].variant_limit;
  darker::game::assign_city_variants(cells, limits);
  darker::resources::scenario_resource const scenario{archives.load({.archive{4}, .slot{0}})};
  darker::resources::campaign_resources campaign{archives};
  for(size_t mission{0}; mission < 15; ++mission) {
    auto const &scenario{campaign.scenario(static_cast<uint8_t>(mission + 1))};
    auto const record_index{darker::resources::select_campaign_stage(static_cast<uint8_t>(mission + 1)).record};
    auto const &record{scenario.records()[record_index]};
    darker::game::mission_combat combat{darker::game::make_scenario_group(record.groups[0], bank, 1, 0, record.shared.offset)};
    combat.reserves = darker::game::make_scenario_group(record.groups[1],bank,static_cast<uint8_t>(1 + record.groups[0].objects.size()),0,record.shared.offset);
    darker::game::player_flight player;
    auto &caero{std::get<darker::game::caero_flight_state>(player.craft)};
    caero.flying = true;
    caero.energy.reserve = 0xcfff;
    combat.primary_weapon = 1;
    combat.difficulty = static_cast<uint8_t>((mission + 1)*2);
    darker::resources::font_resource const fonts{archives.load({.archive{0}, .slot{29}})};
    auto const text{scenario.language(record_index, darker::resources::scenario_language::english)};
    darker::presentation::player briefing{archives,fonts,scenario,record_index};
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
      context.object_counter = static_cast<uint8_t>(combat.completed_objectives);
      context.messages.clear();
      darker::game::advance_mission_script(script, context);
      for(auto const &event : context.messages) {
        if(mission < 3 && !context.objectives_complete) throw std::runtime_error{"Return message preceded objective completion"};
        std::string const actual{reinterpret_cast<char const *>(text.data() + event.offset), event.length};
        if(mission < 3 && actual != std::array{"Well done- you can return to base.","Mission accomplished. Return to base.","Good job, Tolly. Return to base."}[mission]) throw std::runtime_error{"Incorrect first-mission return message"};
        constexpr std::array final_messages{"Well done- you can return to base.","Mission accomplished. Return to base.",
          "Good job, Tolly. Return to base.","all targets are clear.","Mission complete- come back to base.",
          "Well done- you can return to base.","Return to Hemmersan.","Mission complete- come back to base.","Good job, Tolly. Return to base.","Return to base for a mission update.","Mission accomplished. Return to base.","Good job, Tolly. Return to base.","Return to Hemmersan, Tolly.","Mission complete- come back to base.","Return to base immediately, Tolly."};
        message |= actual == final_messages[mission];
      }
      if(message && script.stopped) break;
    }
    if(!message || !script.stopped || combat.completed_objectives != std::array{2u,2u,3u,5u,3u,5u,8u,8u,4u,7u,2u,1u,2u,2u,1u}[mission] || combat.remaining_objectives() != 0 || player.lifecycle.crashing) {
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
  {
    auto const &transfer{campaign.scenario(16)};
    auto const &record{transfer.records()[7]};
    darker::resources::font_resource const fonts{archives.load({0,29})};
    darker::presentation::player briefing{archives,fonts,transfer,7};
    do { briefing.advance(4000); } while(briefing.continue_page());
    darker::game::mission_combat traffic{darker::game::make_scenario_group(record.groups[0],bank,1,0,record.shared.offset)};
    if(briefing.departure_destination != 0x3064 || traffic.actors.size() != 4 || traffic.remaining_objectives() != 0 || !record.groups[1].objects.empty()) {
      throw std::runtime_error{"Mission sixteen did not select the original transfer destination"};
    }
    auto transfer_cells{darker::game::make_city_map(archives.load({0,68}),true)};
    darker::game::player_flight player;
    darker::game::hangar_state hangar{.next_return_site{briefing.departure_destination}};
    darker::game::initialise_caero_hangar(player,transfer_cells,hangar,bank.header_at(bank.special_models()[25]).height);
    for(uint32_t clock{8}; clock <= 4000; clock += 8) traffic.advance(player,transfer_cells,bank,clock,8,0,false,transfer.bytes(record.shared));
    // Isolate the departure boundary and destination handoff from manual navigation.
    player.pose().position[1] -= 768;
    darker::game::advance_hangar_departure(player,transfer_cells,hangar,8);
    if(hangar.return_site != 0x3064 || (player.lifecycle.flags & 16)
      || transfer_cells[113*128+49].state != 0 || transfer_cells[48*128+50].state != 0) {
      throw std::runtime_error{"Hangar departure did not close the old site before changing destination"};
    }
    darker::game::mission_script script{.continuation{*record.player_program - record.shared.offset}};
    darker::game::mission_context context{.program{transfer.bytes(record.shared)},.cells{transfer_cells},.objectives_complete{true}};
    darker::game::advance_mission_script(script,context);
    if(!script.stopped) throw std::runtime_error{"Transfer mission script did not reach its native stop"};
    player.pose().position = {50*256+128,48*256+152-700,500};
    player.pose().angles = {0x8000,0,0};
    if(!darker::game::begin_hangar_return(player,transfer_cells,hangar,true)) throw std::runtime_error{"Mission sixteen refused its destination hangar"};
    for(unsigned int frame{0}; frame < 2000 && hangar.returning != darker::game::hangar_return_phase::complete; ++frame) {
      darker::game::advance_hangar_return(player,hangar,8,static_cast<uint16_t>(frame*8));
      darker::game::advance_hangar_departure(player,transfer_cells,hangar,8);
    }
    if(hangar.returning != darker::game::hangar_return_phase::complete || hangar.return_site != 0x3064) {
      throw std::runtime_error{"Transfer mission did not complete at the destination hangar"};
    }
    std::cout << "Mission sixteen: briefing destination, departure handoff and docking at cell (50,48) verified." << std::endl;
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
  {
    darker::resources::geometry_bank const tunnel_bank{archives.load({0,32})};
    darker::resources::scenario_resource const tunnel_scenario{archives.load({4,2})};
    auto const &record{tunnel_scenario.records()[0]};
    auto const placement{std::ranges::find(record.groups[0].objects,29,&darker::resources::scenario_placement::definition_slot)};
    if(placement == record.groups[0].objects.end()) throw std::runtime_error{"First tunnel has no Wrecker"};
    auto const model{tunnel_bank.special_models()[29]};
    auto const wrecker{darker::game::make_scenario_actor(*placement,darker::game::original_object_definitions[29],model,tunnel_bank.header_at(model).height,4,2,record.shared.offset)};
    darker::game::mission_combat tunnel{{wrecker}};
    darker::game::player_flight observer;
    auto tunnel_cells{darker::game::make_city_map(archives.load({0,70}),false)};
    std::array<size_t,3> const doors{56*128+61,49*128+65,41*128+62};
    std::array<uint32_t,6> const times{57344,73728,155648,172032,253952,270336};
    size_t events{0};
    bool sparks{false}, bursts{false};
    for(uint32_t clock{0}; clock <= 280000; clock += 512) {
      std::array<uint8_t,3> const previous{tunnel_cells[doors[0]].state,tunnel_cells[doors[1]].state,tunnel_cells[doors[2]].state};
      tunnel.advance(observer,tunnel_cells,tunnel_bank,clock,512,0,false,tunnel_scenario.bytes(record.shared));
      sparks |= !tunnel.effects.trails.empty();
      bursts |= !tunnel.effects.emitters.empty();
      for(size_t door{0}; door < doors.size(); ++door) {
        if(tunnel_cells[doors[door]].state == previous[door]) continue;
        if(events >= times.size() || clock != times[events] || door != events/2
          || tunnel_cells[doors[door]].state != ((events & 1) ? 0x40 : 0x20)) {
          throw std::runtime_error{"Wrecker door destruction differs from native route events"};
        }
        ++events;
      }
    }
    if(events != times.size() || !sparks || !bursts) throw std::runtime_error{"Wrecker route omitted door damage or cutting effects"};
    std::cout << "First tunnel Wrecker: all six native door transitions, raised motion, sparks and bursts verified." << std::endl;
  }
  {
    darker::game::tunnel_network const network{archives.load({0,78})};
    darker::resources::geometry_bank const underground_bank{archives.load({0,32})};
    for(auto const &sample : darker::test_reference::tunnel_placement_samples) {
      auto const &source{campaign.scenario(static_cast<uint8_t>(sample[0]*8 + sample[1] + 1))};
      auto const &record{source.records()[sample[1]]};
      auto const map{darker::game::make_city_map(archives.load({0,70u + (record.configuration >> 4)}),false)};
      unsigned int index{0};
      darker::resources::scenario_placement const *placement{nullptr};
      for(auto const &group : record.groups) for(auto const &object : group.objects) {
        if(++index == sample[2]) placement = &object;
      }
      if(!placement || placement->definition_slot != sample[3]) throw std::runtime_error{"Native tunnel placement refers to a different scenario object"};
      auto const model{underground_bank.special_models()[sample[3]]};
      auto const height{underground_bank.header_at(model).height};
      auto const actor{darker::game::make_scenario_actor(*placement,darker::game::original_object_definitions[sample[3]],
        model,height,static_cast<uint8_t>(sample[2]),2,record.shared.offset,darker::game::tunnel_setup{network,map})};
      if(actor.pose.position != std::array<uint16_t,3>{sample[4],sample[5],static_cast<uint16_t>(sample[6] - height)}
        || actor.pose.angles[0] != sample[7] || !actor.tunnel || actor.tunnel->route != sample[8]
        || actor.parameters.update_entry != 0x8609) {
        throw std::runtime_error{"Underground actor differs from native route placement: archive=" + std::to_string(sample[0])
          + ", record=" + std::to_string(sample[1]) + ", object=" + std::to_string(sample[2])};
      }
    }
    std::array<std::optional<darker::game::city_map>,8> maps;
    for(auto const &sample : darker::test_reference::tunnel_connection_samples) {
      auto &map{maps.at(sample[0] - 70)};
      if(!map) map = darker::game::make_city_map(archives.load({0,sample[0]}),false);
      auto const result{network.connect(*map,{sample[1],static_cast<uint8_t>(sample[2])},static_cast<uint8_t>(sample[3]))};
      if(result.cell != sample[4] || result.route != sample[5]) {
        throw std::runtime_error{"Tunnel connection differs from native: map=" + std::to_string(sample[0])
          + ", cell=" + std::to_string(sample[1]) + ", route=" + std::to_string(sample[2]) + ", heading=" + std::to_string(sample[3])
          + ", actual=" + std::to_string(result.cell) + "/" + std::to_string(result.route)
          + ", expected=" + std::to_string(sample[4]) + "/" + std::to_string(sample[5])};
      }
    }
    for(auto const &sample : darker::test_reference::tunnel_trace_samples) {
      auto &map{maps.at(sample[0] - 70)};
      if(!map) map = darker::game::make_city_map(archives.load({0,sample[0]}),false);
      auto const result{network.trace(*map,{sample[1],static_cast<uint8_t>(sample[2])},
        {sample[4],sample[5],0},sample[6],static_cast<uint8_t>(sample[3]))};
      if(!result || result->progress != sample[7] || result->connection.cell != sample[8] || result->connection.route != sample[9]
        || result->target != std::array<uint16_t,3>{sample[10],sample[11],sample[12]}) {
        throw std::runtime_error{"Tunnel lookahead differs from native: map=" + std::to_string(sample[0])
          + ", cell=" + std::to_string(sample[1]) + ", route=" + std::to_string(sample[2]) + ", lookahead=" + std::to_string(sample[6])
          + ", expected=" + std::to_string(sample[7]) + "/" + std::to_string(sample[8]) + "/" + std::to_string(sample[9])
          + "/" + std::to_string(sample[10]) + "/" + std::to_string(sample[11]) + "/" + std::to_string(sample[12])
          + ", actual=" + (result ? std::to_string(result->progress) + "/" + std::to_string(result->connection.cell) + "/" + std::to_string(result->connection.route)
          + "/" + std::to_string(result->target[0]) + "/" + std::to_string(result->target[1]) + "/" + std::to_string(result->target[2]) : "none")};
      }
    }
    for(auto const &sample : darker::test_reference::tunnel_reacquisition_samples) {
      auto &map{maps.at(sample[0] - 70)};
      if(!map) map = darker::game::make_city_map(archives.load({0,sample[0]}),false);
      auto const result{network.reacquire(*map,{sample[1],static_cast<uint8_t>(sample[2])},
        {sample[4],sample[5],sample[6]},static_cast<uint8_t>(sample[3]))};
      if(result.has_value() != (sample[7] != 0) || (result && (result->cell != sample[8] || result->route != sample[9]))) {
        throw std::runtime_error{"Tunnel reacquisition differs from native: map=" + std::to_string(sample[0])
          + ", source=" + std::to_string(sample[1]) + ", route=" + std::to_string(sample[2])
          + ", position=" + std::to_string(sample[4]) + "/" + std::to_string(sample[5]) + "/" + std::to_string(sample[6])
          + ", expected=" + std::to_string(sample[7]) + "/" + std::to_string(sample[8]) + "/" + std::to_string(sample[9])
          + ", actual=" + (result ? std::to_string(result->cell) + "/" + std::to_string(result->route) : "none")};
      }
    }
    for(auto const &sample : darker::test_reference::tunnel_navigation_samples) {
      auto &map{maps.at(sample[0] - 70)};
      if(!map) map = darker::game::make_city_map(archives.load({0,sample[0]}),false);
      darker::game::scenario_actor actor;
      actor.current_cell = sample[1];
      actor.target_token = sample[3];
      actor.pose.angles[0] = sample[4];
      actor.tunnel = darker::game::tunnel_actor_state{.route{static_cast<uint8_t>(sample[2])},
        .progress{sample[6]},.oscillation{static_cast<uint8_t>(sample[5])}};
      auto const preferred{darker::game::choose_tunnel_heading(actor,*map,network)};
      if(actor.current_cell != sample[7] || actor.tunnel->route != sample[8] || actor.tunnel->oscillation != sample[9]
        || actor.tunnel->progress != sample[10] || preferred != sample[11]) {
        throw std::runtime_error{"Underground heading choice differs from native: map=" + std::to_string(sample[0])
          + ", cell=" + std::to_string(sample[1]) + ", route=" + std::to_string(sample[2]) + ", target=" + std::to_string(sample[3])
          + ", expected=" + std::to_string(sample[7]) + "/" + std::to_string(sample[8]) + "/" + std::to_string(sample[9])
          + "/" + std::to_string(sample[10]) + "/" + std::to_string(sample[11])
          + ", actual=" + std::to_string(actor.current_cell) + "/" + std::to_string(actor.tunnel->route) + "/" + std::to_string(actor.tunnel->oscillation)
          + "/" + std::to_string(actor.tunnel->progress) + "/" + std::to_string(preferred)};
      }
    }
    for(auto const &sample : darker::test_reference::tunnel_entry_samples) {
      darker::game::player_flight player;
      darker::game::initialise_tunnel_entry(player,static_cast<uint16_t>(sample[1]),static_cast<uint8_t>(sample[2]),static_cast<int16_t>(sample[3]));
      auto const &craft{std::get<darker::game::caero_flight_state>(player.craft)};
      std::array<int,10> const actual{player.pose().position[0],player.pose().position[1],player.pose().position[2],
        player.pose().angles[0],player.pose().angles[1],player.lifecycle.flags,player.forward_setting,
        craft.energy.reserve,craft.energy.boost,player.tunnel->connection.cell};
      if(!std::equal(actual.begin(),actual.end(),sample.begin()+4)) throw std::runtime_error{"Underground entry differs from native placement"};
    }
    for(auto const &sample : darker::test_reference::tunnel_portal_samples) {
      auto &map{maps.at(static_cast<size_t>(sample[0] - 70))};
      if(!map) map = darker::game::make_city_map(archives.load({0,static_cast<unsigned int>(sample[0])}),false);
      auto cells{*map};
      darker::game::player_flight player;
      player.tunnel.emplace();
      player.tunnel->connection.route = static_cast<uint8_t>(sample[4]);
      player.tunnel->lookahead = static_cast<uint16_t>(sample[7]);
      player.pose().position = {static_cast<uint16_t>(sample[2]*256+128),static_cast<uint16_t>(sample[3]*256+128),0};
      player.lifecycle.flags = static_cast<uint8_t>(sample[5]);
      player.forward_setting = static_cast<uint16_t>(sample[6]);
      darker::game::hangar_state portal{.return_site{static_cast<uint16_t>(sample[1])},.next_return_site{static_cast<uint16_t>(sample[1])}};
      darker::game::update_tunnel_portal(player,cells,portal,static_cast<uint16_t>(sample[8]));
      auto const centre{(sample[1] >> 8)*128 + ((sample[1] & 255) >> 1)};
      std::array<int,9> const actual{player.lifecycle.flags,player.forward_setting,player.tunnel->lookahead,
        portal.returning == darker::game::hangar_return_phase::approaching ? 0x7d71 : 0xd510,portal.extension,
        cells[centre-128].state,cells[centre].state,cells[centre+128].state,portal.return_site};
      if(!std::equal(actual.begin(),actual.end(),sample.begin()+9)) {
        throw std::runtime_error{"Underground portal differs from native: map=" + std::to_string(sample[0])
          + ", position=" + std::to_string(sample[2]) + "/" + std::to_string(sample[3]) + ", route=" + std::to_string(sample[4])
          + ", flags=" + std::to_string(sample[5])};
      }
    }
    {
      darker::game::player_flight player;
      player.tunnel.emplace();
      player.pose().position = {12928,12530,800};
      darker::game::hangar_state portal{.return_site{0x3064},.returning{darker::game::hangar_return_phase::approaching}};
      for(auto const &expected : darker::test_reference::tunnel_return_samples) {
        darker::game::advance_hangar_return(player,portal,8,static_cast<uint16_t>(expected[0]));
        auto const &craft{std::get<darker::game::caero_flight_state>(player.craft)};
        std::array<int,14> const actual{player.pose().position[0],player.pose().position[1],player.pose().position[2],
          player.pose().fractions[0],player.pose().fractions[1],player.pose().fractions[2],
          craft.damage.rotation.pitch,craft.damage.rotation.turn,player.pose().angles[0],player.pose().angles[1],player.pose().angles[2],
          player.pose().speed,portal.extension,static_cast<int>(portal.returning)};
        for(size_t field{0}; field < actual.size(); ++field) if(actual[field] != expected[field+1]) {
          throw std::runtime_error{"Tunnel return differs from native: clock=" + std::to_string(expected[0])
            + ", field=" + std::to_string(field) + ", actual=" + std::to_string(actual[field]) + ", expected=" + std::to_string(expected[field+1])};
        }
      }
    }
    unsigned int flight_sample{0};
    for(auto const &sample : darker::test_reference::tunnel_flight_samples) {
      auto const &before{sample.before};
      darker::game::tunnel_flight_state flight;
      darker::game::caero_flight_state craft;
      craft.pose.position = {before[0],before[1],before[2]};
      craft.pose.angles = {before[3],before[4],before[5]};
      craft.pose.speed = before[6];
      flight.heading_rate = before[7];
      craft.damage.rotation = {before[8],before[9]};
      craft.horizontal_velocity = before[10];
      craft.vertical_velocity = before[11];
      flight.connection = {before[12],static_cast<uint8_t>(before[14])};
      flight.progress = before[13];
      craft.pose.fractions = {static_cast<uint8_t>(before[15]),static_cast<uint8_t>(before[16]),static_cast<uint8_t>(before[17])};
      flight.filtered_pitch = before[18];
      flight.filtered_bank = before[19];
      flight.off_route_time = before[20];
      flight.resistance = before[21];
      craft.energy.boost = before[22];
      craft.energy.incoming_display = static_cast<uint8_t>(before[23]);
      flight.aiming = before[24] != 0;
      flight.aim_heading = before[25];
      flight.aim_pitch = before[26];
      flight.aim_heading_rate = before[27];
      flight.aim_pitch_rate = before[28];
      craft.energy.reserve = before[29];
      craft.energy.reserve_display = static_cast<uint8_t>(before[30]);
      craft.repair_phase = before[31];
      craft.damage.damage = before[32];
      flight.lookahead = before[33];
      auto const &input{sample.input};
      auto controller{darker::game::player_flight{}};
      bool const check_controller{flight_sample >= 1024 && flight_sample < 1056};
      if(check_controller) {
        controller.craft = craft;
        controller.tunnel = flight;
        controller.forward_setting = input[3];
        controller.engine_flags = static_cast<uint8_t>(input[5]);
        auto city{*maps[0]};
        auto const contact{controller.advance({},input[6] != 0,input[0],static_cast<uint16_t>(flight_sample*input[0]),underground_bank,city,&network)};
        if(contact.contact != darker::game::city_contact::none) throw std::runtime_error{"Hands-off tunnel flight unexpectedly touches geometry"};
      }
      auto const advance{input[8] ? darker::game::advance_tunnel_flight : darker::game::advance_tunnel_motion};
      advance(craft,flight,{.pitch_reference{input[1]},.bank_reference{input[2]},.pitch_drive{input[7]},.forward_setting{input[3]},
        .angular_response{input[4]},.cell_collision_marker{static_cast<uint8_t>(input[9])},.engine{input[5] != 0},.brake{input[6] != 0}},input[0],*maps[0],network);
      std::array<uint16_t,34> const actual{craft.pose.position[0],craft.pose.position[1],craft.pose.position[2],
        craft.pose.angles[0],craft.pose.angles[1],craft.pose.angles[2],craft.pose.speed,flight.heading_rate,
        craft.damage.rotation.pitch,craft.damage.rotation.turn,craft.horizontal_velocity,craft.vertical_velocity,
        flight.connection.cell,flight.progress,flight.connection.route,craft.pose.fractions[0],craft.pose.fractions[1],craft.pose.fractions[2],
        flight.filtered_pitch,flight.filtered_bank,flight.off_route_time,flight.resistance,craft.energy.boost,craft.energy.incoming_display,
        static_cast<uint16_t>(flight.aiming),flight.aim_heading,flight.aim_pitch,flight.aim_heading_rate,flight.aim_pitch_rate,
        craft.energy.reserve,craft.energy.reserve_display,craft.repair_phase,craft.damage.damage,flight.lookahead};
      for(unsigned int field{0}; field < actual.size(); ++field) if(actual[field] != sample.after[field]) {
        throw std::runtime_error{"Tunnel player flight differs from native: sample=" + std::to_string(flight_sample)
          + ", field=" + std::to_string(field) + ", actual=" + std::to_string(actual[field]) + ", expected=" + std::to_string(sample.after[field])};
      }
      if(check_controller) {
        auto const &controlled{std::get<darker::game::caero_flight_state>(controller.craft)};
        if(controlled.pose.position != craft.pose.position || controlled.pose.angles != craft.pose.angles
          || controlled.pose.fractions != craft.pose.fractions || controlled.pose.speed != craft.pose.speed
          || controlled.energy.boost != craft.energy.boost || controller.tunnel->connection.route != flight.connection.route) {
          throw std::runtime_error{"Player controller differs from native underground flight"};
        }
      }
      ++flight_sample;
    }
    auto const &underground_record{campaign.scenario(17).records()[0]};
    {
      auto scripted{darker::game::make_scenario_group(underground_record.groups[0],underground_bank,1,2,
        underground_record.shared.offset,darker::game::tunnel_setup{network,*maps[0]})};
      std::erase_if(scripted,[](auto const &actor){ return actor.category != darker::game::actor_category::air; });
      darker::game::mission_combat underground{scripted};
      darker::game::player_flight observer;
      observer.pose().position = {13952,14464,512};
      observer.tunnel.emplace();
      auto geometry{*maps[0]};
      size_t sample_index{0};
      for(unsigned int frame{0}; frame < 512; ++frame) {
        underground.advance(observer,geometry,underground_bank,frame*8,8,0,false,
          campaign.scenario(17).bytes(underground_record.shared),50,&network);
        if(underground.actors.size() != 3) throw std::runtime_error{"Scripted underground aircraft were unexpectedly removed"};
        for(auto const &actor : underground.actors) {
          auto const &expected{darker::test_reference::tunnel_scripted_actor_samples[sample_index++]};
          std::array<uint16_t,23> const actual{actor.pose.position[0],actor.pose.position[1],actor.pose.position[2],
            actor.pose.angles[0],actor.pose.angles[1],actor.pose.angles[2],actor.pose.speed,
            actor.attitude.pitch_rate,actor.attitude.bank_rate,actor.awareness.level,actor.awareness.cooldown,
            actor.current_cell,actor.target_token,actor.tunnel->progress,actor.tunnel->route,actor.tunnel->oscillation,
            actor.pose.fractions[0],actor.pose.fractions[1],actor.pose.fractions[2],actor.flags,actor.script.deadline,
            static_cast<uint16_t>(actor.script.stopped ? 65535 : actor.script.continuation),static_cast<uint16_t>(actor.script.checkpoint)};
          for(unsigned int field{0}; field < actual.size(); ++field) if(actual[field] != expected[field]) {
            throw std::runtime_error{"Scripted tunnel actor differs from native: frame=" + std::to_string(frame)
              + ", actor=" + std::to_string(actor.index) + ", field=" + std::to_string(field)
              + ", actual=" + std::to_string(actual[field]) + ", expected=" + std::to_string(expected[field])};
          }
        }
      }
    }
    auto actors{darker::game::make_scenario_group(underground_record.groups[0],underground_bank,1,2,
      underground_record.shared.offset,darker::game::tunnel_setup{network,*maps[0]})};
    std::erase_if(actors,[](auto const &actor){ return actor.category != darker::game::actor_category::air; });
    if(actors.size() != 3) throw std::runtime_error{"First tunnel motion fixture requires its three initial aircraft"};
    std::array<uint16_t,3> starts{actors[2].current_cell,actors[1].current_cell,actors[0].current_cell};
    darker::game::object_pose player{.position{13952,14464,512}};
    size_t sample_index{0};
    for(unsigned int frame{0}; frame < 512; ++frame) {
      if(frame == 128) for(auto &actor : actors) actor.target_token = starts[actor.index % 3];
      if(frame >= 256) {
        player.position = actors.back().pose.position;
        player.position[0] += 100;
      }
      for(auto &actor : actors) {
        darker::game::advance_tunnel_actor(actor,player,actors,*maps[0],network,8);
        std::array<uint16_t,19> const actual{actor.pose.position[0],actor.pose.position[1],actor.pose.position[2],
          actor.pose.angles[0],actor.pose.angles[1],actor.pose.angles[2],actor.pose.speed,actor.attitude.pitch_rate,actor.attitude.bank_rate,
          actor.awareness.level,actor.awareness.cooldown,actor.current_cell,actor.target_token,actor.tunnel->progress,
          actor.tunnel->route,actor.tunnel->oscillation,actor.pose.fractions[0],actor.pose.fractions[1],actor.pose.fractions[2]};
        auto const &expected{darker::test_reference::tunnel_actor_samples[sample_index++]};
        for(size_t field{0}; field < actual.size(); ++field) if(actual[field] != expected[field]) {
          throw std::runtime_error{"Underground motion differs from native: frame=" + std::to_string(frame)
            + ", actor=" + std::to_string(actor.index) + ", field=" + std::to_string(field)
            + ", actual=" + std::to_string(actual[field]) + ", expected=" + std::to_string(expected[field])};
        }
      }
    }
    std::cout << "All 1536 underground actor updates match native steering, route traversal and nearby-aircraft responses." << std::endl;
    std::cout << "All 2528 underground heading/junction decisions match native execution." << std::endl;
    std::cout << "All 1264 tunnel projection/lookahead targets match native route crossings." << std::endl;
    std::cout << "All 1264 tunnel connection choices match native endpoint, height and heading selection." << std::endl;
    std::cout << "All 158 underground moving-object placements match native route snapping and direction selection." << std::endl;
  }
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
  if(!convoy.actors.empty() || calls != 3120 || convoy.completed_objectives != 0) {
    throw std::runtime_error{"Fourth-mission flatbed removal differs from native route timing"};
  }
  for(size_t i{0}; i < cells.size(); ++i) {
    if(cells[i].type != before[i].type || cells[i].state != before[i].state) throw std::runtime_error{"Flatbed movement changed the city"};
  }
  {
    darker::resources::scenario_resource const supplementary{archives.load({4,15})};
    auto const &record{supplementary.records()[7]};
    auto blackout_cells{darker::game::make_city_map(archives.load({0,68}),true)};
    for(auto &cell : blackout_cells) if(cell.type == 1) cell.state = 255;
    darker::game::beacon_changes fade;
    darker::game::mission_script script{.continuation{record.entry_offset - record.shared.offset}};
    darker::game::mission_context context{.program{supplementary.bytes(record.shared)},
      .text{supplementary.language(7,darker::resources::scenario_language::english)},.cells{blackout_cells},.time_multiplier{record.time_multiplier}};
    context.change_beacons = [&](uint8_t op, uint8_t origin, uint8_t count){ fade.command(op,origin,count,static_cast<uint16_t>(context.clock),{}); };
    unsigned int messages{0}, last_change{0};
    for(uint32_t clock{0}; clock <= 40000; clock += 16) {
      auto const previous{blackout_cells};
      context.clock = clock;
      fade.advance(blackout_cells,static_cast<uint16_t>(clock));
      for(size_t i{0}; i < blackout_cells.size(); ++i) if(previous[i].state != blackout_cells[i].state) last_change = clock;
      context.messages.clear();
      darker::game::advance_mission_script(script,context);
      messages += static_cast<unsigned int>(context.messages.size());
    }
    auto const dark{std::ranges::count_if(blackout_cells,[](auto cell){ return cell.type == 1 && cell.state == 0; })};
    if(dark != 224 || blackout_cells[126*128+72].state != 255 || last_change != 12512 || messages != 9) {
      throw std::runtime_error{"Shared blackout differs from its native 224-tower, nine-message timeline"};
    }
    std::cout << "Shared blackout: 224 towers fade, nine messages and final change at tick 12512 match native execution." << std::endl;
  }
  std::cout << "Fourth-mission flatbed traversed its original route and removed itself after 155,950 ticks at 50-tick sampling." << std::endl;

}
