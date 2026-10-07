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
#include "game/mission_exchange.h"
#include "game/object_definitions.h"
#include "game/projectile_steering.h"
#include "game/scenario_world.h"
#include "game/tunnel_flight.h"
#include "game/tunnel_navigation.h"
#include "game/tunnel_portal.h"
#include "graphics/city_scene.h"
#include "graphics/formatted_text.h"
#include "maths/sine_table.h"
#include "presentation/player.h"
#include "reference/actor_admission_samples.h"
#include "reference/aircraft_bomb_samples.h"
#include "reference/aircraft_contact_samples.h"
#include "reference/aircraft_spawning_samples.h"
#include "reference/target_acquisition_samples.h"
#include "reference/tunnel_actor_samples.h"
#include "reference/tunnel_connection_samples.h"
#include "reference/tunnel_flight_samples.h"
#include "reference/tunnel_launch_samples.h"
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
  for(auto const &sample : darker::test_reference::actor_admission_samples) {
    darker::game::scenario_actor source;
    source.index = 1;
    source.definition_slot = 20;
    darker::game::apply_object_definition(source.parameters,darker::game::original_object_definitions[20],bank.special_models()[20]);
    source.parameters.update_entry = 0x8823;
    source.pose.position = {12000,12000,4000};
    source.previous_position = source.pose.position;
    source.script.continuation = 0;
    source.script.deadline = 1000;
    auto reserve{source};
    reserve.index = 2;
    reserve.category = sample[1] ? darker::game::actor_category::ground : darker::game::actor_category::air;
    reserve.pose.position = {24000,24000,4000};
    reserve.previous_position = reserve.pose.position;
    reserve.script.continuation = 2;
    darker::game::mission_combat combat{sample[0] ? std::vector<darker::game::scenario_actor>{} : std::vector{source}};
    combat.reserves.push_back(reserve);
    darker::game::player_flight player;
    player.pose().position = {32000,32000,4000};
    darker::game::city_map empty{};
    std::array<std::byte,3> const program{static_cast<std::byte>(sample[1] ? 10 : 9),std::byte{1},std::byte{0x23}};
    for(size_t frame{0}; frame < 2; ++frame) {
      combat.advance(player,empty,bank,1000+static_cast<uint32_t>(frame),1,0,false,program);
      if(sample[0] && frame == 0) combat.activate_reserves(reserve.category,1,player.pose(),1000);
      auto const admitted{std::ranges::find(combat.actors,uint8_t{2},&darker::game::scenario_actor::index)};
      if(admitted == combat.actors.end() || admitted->script.stopped != (sample[2+frame] != 0)) {
        throw std::runtime_error{"Reserve callback ran in a different frame from the native list traversal: player="+std::to_string(sample[0])+", ground="+std::to_string(sample[1])+", frame="+std::to_string(frame)+", admitted="+std::to_string(admitted != combat.actors.end())+", stopped="+std::to_string(admitted == combat.actors.end() ? false : admitted->script.stopped)};
      }
    }
  }
  for(auto const &s : darker::test_reference::aircraft_contact_samples) {
    darker::game::mission_combat combat{{}};
    combat.random_state = static_cast<uint16_t>(s[0]);
    for(size_t i{0}; i < 3; ++i) {
      auto const at{5+i*9};
      darker::game::scenario_actor actor;
      actor.index = static_cast<uint8_t>(i+1);
      actor.definition_slot = static_cast<uint8_t>(s[at]);
      actor.parameters.definition = &darker::game::original_object_definitions[actor.definition_slot];
      actor.parameters.model_token = bank.special_models()[actor.definition_slot];
      actor.parameters.update_entry = 0x8823;
      actor.flags = static_cast<uint8_t>(s[at+1]);
      for(size_t axis{0}; axis < 3; ++axis) {
        actor.pose.position[axis] = static_cast<uint16_t>(s[at+2+axis]);
        actor.previous_position[axis] = static_cast<uint16_t>(i == 0 ? s[2+axis] : s[at+2+axis]);
      }
      actor.attitude = {static_cast<uint16_t>(s[at+5]),static_cast<uint16_t>(s[at+6])};
      actor.awareness = {static_cast<uint16_t>(s[at+7]),static_cast<uint16_t>(s[at+8])};
      combat.actors.push_back(actor);
    }
    combat.collide_aircraft({},bank,0x20,static_cast<uint16_t>(s[1]),s[64] != 0);
    for(size_t i{0}; i < 3; ++i) {
      auto const &actor{combat.actors[i]};
      std::array<unsigned int,10> const actual{actor.pose.position[0],actor.pose.position[1],actor.pose.position[2],
        actor.attitude.pitch_rate,actor.attitude.bank_rate,actor.awareness.level,actor.awareness.cooldown,
        actor.parameters.update_entry,actor.expiry,actor.flags};
      for(size_t field{0}; field < actual.size(); ++field) {
        if(actual[field] != s[32+i*10+field]) throw std::runtime_error{"Aircraft contact differs from native: actor="+std::to_string(i)
          +", field="+std::to_string(field)+", seed="+std::to_string(s[0])+", actual="+std::to_string(actual[field])+", expected="+std::to_string(s[32+i*10+field])};
      }
    }
    if(combat.random_state != s[62]) throw std::runtime_error{"Aircraft contact random sequence differs from native"};
  }
  std::cout << "All 512 ordered aircraft contacts match native clipping, paired damage and random state." << std::endl;
  for(auto const &s : darker::test_reference::target_acquisition_samples) {
    auto const word{[](int const value){ return static_cast<uint16_t>(value); }};
    darker::game::city_map city{};
    city[50*128+50] = {static_cast<uint8_t>(s[0]),static_cast<uint8_t>(s[1])};
    darker::game::object_pose const player{.position{word(s[2]),word(s[3]),word(s[4])},.angles{word(s[5]),word(s[6]),0}};
    std::array<darker::game::scenario_actor,2> actors;
    for(size_t i{0}; i < actors.size(); ++i) {
      actors[i].index = static_cast<uint8_t>(i+1);
      actors[i].parameters.model_token = bank.special_models()[19];
      actors[i].pose.position = {word(s[7+i*3]),word(s[8+i*3]),word(s[9+i*3])};
    }
    auto const token{darker::game::acquire_caero_target(player,actors,city,bank,0x20)};
    if(token != s[13]) throw std::runtime_error{"Target acquisition differs from native: actual=" + std::to_string(token)
      + ", expected=" + std::to_string(s[13]) + ", building=" + std::to_string(s[0]) + ", height=" + std::to_string(s[4])};
  }
  auto cells{darker::game::make_city_map(archives.load({.archive{0}, .slot{68}}), true)};
  std::array<uint8_t, 256> limits{};
  for(size_t i{0}; i < bank.city_types().size(); ++i) limits[i + 1] = bank.city_types()[i].variant_limit;
  darker::game::assign_city_variants(cells, limits);
  darker::resources::scenario_resource const scenario{archives.load({.archive{4}, .slot{0}})};
  darker::resources::campaign_resources campaign{archives};
  for(auto const &v : darker::test_reference::aircraft_spawning_samples) {
    darker::game::aircraft_spawning state{.enabled{v[5] != 0}};
    state.timers[0] = static_cast<uint16_t>(v[6]);
    darker::game::city_map city{};
    city[(v[0] >> 8)*128 + (v[0] & 127)] = {76,static_cast<uint8_t>(v[1])};
    darker::game::object_pose const player{.position{static_cast<uint16_t>(v[2]*256),static_cast<uint16_t>(v[3]*256),0}};
    std::vector<darker::game::scenario_actor> active, free;
    if(v[4]) {
      free.emplace_back();
      free.back().script.checkpoint = 0xe800;
      darker::game::apply_object_definition(free.back().parameters,darker::game::original_object_definitions[20],bank.special_models()[20]);
    }
    uint16_t random{static_cast<uint16_t>(v[9])};
    darker::game::advance_aircraft_spawning(state,active,free,city,bank,player,static_cast<uint16_t>(v[7]),static_cast<uint16_t>(v[8]),random);
    if(active.size() != v[11] || state.timers[0] != v[12] || random != v[13]) {
      throw std::runtime_error{"Warehouse admission or timer differs from native: site=" + std::to_string(v[0])
        + ", flags=" + std::to_string(v[1]) + ", player=" + std::to_string(v[2]) + "," + std::to_string(v[3])
        + ", step=" + std::to_string(v[8]) + ", initial timer=" + std::to_string(v[6])
        + ", active=" + std::to_string(active.size()) + "/" + std::to_string(v[11])
        + ", timer=" + std::to_string(state.timers[0]) + "/" + std::to_string(v[12])
        + ", random=" + std::to_string(random) + "/" + std::to_string(v[13])};
    }
    if(active.empty()) continue;
    auto const &actor{active.front()};
    std::array<uint16_t,15> const actual{actor.pose.position[0],actor.pose.position[1],actor.pose.position[2],
      actor.pose.angles[0],actor.pose.angles[1],actor.pose.angles[2],actor.pose.speed,actor.selected_target,actor.target_token,
      actor.current_cell,actor.parameters.update_entry,actor.expiry,actor.script.deadline,actor.flags,
      static_cast<uint16_t>(actor.script.continuation == 0xe800)};
    for(size_t i{0}; i < actual.size(); ++i) if(actual[i] != v[i+15]) throw std::runtime_error{"Warehouse placement differs from native: field=" + std::to_string(i)};
  }
  {
    darker::game::scenario_actor actor;
    darker::game::apply_object_definition(actor.parameters,darker::game::original_object_definitions[20],bank.special_models()[20]);
    actor.parameters.update_entry = 0x8ddd;
    actor.flags = 0x50;
    actor.pose = {.position{21632,14328,static_cast<uint16_t>(100-bank.header_at(bank.special_models()[20]).height)},.angles{0x8000,0,0},.speed{100}};
    actor.script.deadline = 1024;
    uint16_t clock{0};
    for(auto const &expected : darker::test_reference::aircraft_departure_samples) {
      clock += 8;
      darker::game::advance_aircraft_departure(actor,clock,8);
      std::array<uint16_t,14> const actual{actor.pose.position[0],actor.pose.position[1],actor.pose.position[2],
        actor.pose.angles[0],actor.pose.angles[1],actor.pose.angles[2],actor.pose.speed,actor.attitude.pitch_rate,actor.attitude.bank_rate,
        actor.parameters.update_entry,actor.pose.fractions[0],actor.pose.fractions[1],actor.pose.fractions[2],actor.flags};
      for(size_t i{0}; i < actual.size(); ++i) if(actual[i] != expected[i]) throw std::runtime_error{"Warehouse departure differs from native: field=" + std::to_string(i)};
    }
  }
  for(unsigned int const bank_id : {30u,31u,32u}) {
    darker::resources::geometry_bank const geometry{archives.load({0,bank_id})};
    darker::game::city_map target_cells{};
    for(auto const &sample : darker::test_reference::building_guidance_samples) {
      if(sample[0] != bank_id) continue;
      auto const cell{static_cast<uint16_t>(sample[3])};
      target_cells[(cell >> 8)*128 + (cell & 127)] = {static_cast<uint8_t>(sample[1]),static_cast<uint8_t>(sample[2])};
      auto const target{darker::game::resolve_map_guidance(cell,target_cells,geometry,bank_id == 30 ? 0x20 : 0x60)};
      if(target.position != std::array<uint16_t,2>{static_cast<uint16_t>(sample[4]),static_cast<uint16_t>(sample[5])}
        || target.height != sample[6] || target.height_extent != sample[7]) throw std::runtime_error{"Building missile guidance differs from native model lookup"};
    }
  }
  {
    // Isolate the three blast-category passes from navigation and projectile-to-surface collisions.
    std::vector<darker::game::scenario_actor> targets(4);
    for(size_t i{0}; i < targets.size(); ++i) {
      auto &actor{targets[i]};
      actor.index = static_cast<uint8_t>(i+1);
      actor.category = i == 0 ? darker::game::actor_category::air : i == 1 ? darker::game::actor_category::ground : darker::game::actor_category::stationary;
      actor.attributes = 1;
      darker::game::apply_object_definition(actor.parameters,darker::game::original_object_definitions[21],bank.special_models()[21]);
      actor.parameters.update_entry = 0;
      actor.pose.position = {static_cast<uint16_t>(i == 3 ? 13000 : 10100),10000,10000};
      actor.previous_position = actor.pose.position;
    }
    darker::game::mission_combat blast{targets};
    darker::game::launch_emitter const emitter{.position{10000,10000,10000},.definition_strength{40}};
    auto *capsule{blast.projectiles.launch({.definition{darker::game::original_object_definitions[2]},.emitter{emitter},.model_token{bank.special_models()[2]},.lifetime{2560}})};
    auto *detonator{blast.projectiles.launch({.definition{darker::game::original_object_definitions[6]},.emitter{emitter},.model_token{bank.special_models()[6]},.lifetime{2560},.target_token{capsule->native_id}})};
    darker::game::city_map empty{};
    darker::game::player_flight observer;
    observer.pose().position = {15000,15000,10000};
    blast.advance(observer,empty,bank,8,8,0,false);
    if(blast.completed_objectives != 3 || blast.remaining_objectives() != 1 || blast.actors.size() != 1 || blast.actors.front().index != 4
      || !(capsule->flags & 8) || !(detonator->flags & 8) || capsule->deadline != 264 || detonator->deadline != 264
      || blast.effects.emitters.size() < 3) throw std::runtime_error{"Paired Dual Launch blast did not damage all three categories and retire both parts"};
  }
  struct combat_case { uint8_t stage; unsigned int removals; char const *message; bool permits_survivors{false}; uint8_t weapon{1}; bool aircraft_trails{true}; };
  constexpr std::array<combat_case,56> cases{{
    combat_case{1,2,"Well done- you can return to base."}, {2,2,"Mission accomplished. Return to base."},
    {3,3,"Good job, Tolly. Return to base."}, {4,5,"all targets are clear."}, {5,3,"Mission complete- come back to base."},
    {6,5,"Well done- you can return to base."}, {7,8,"Return to Hemmersan."}, {8,8,"Mission complete- come back to base."},
    {9,4,"Good job, Tolly. Return to base."}, {10,7,"Return to base for a mission update."},
    {11,2,"Mission accomplished. Return to base."}, {12,1,"Good job, Tolly. Return to base."},
    {13,2,"Return to Hemmersan, Tolly."}, {14,2,"Mission complete- come back to base."},
    {15,1,"Return to base immediately, Tolly."}, {19,3,"Return to Hemmersan."}, {20,5,"Return to Hemmersan."},
    {21,8,"all targets are clear.",false,10}, {22,7,"Return to Hemmersan."},
    {25,7,"Good job, Tolly. Return to base."}, {26,12,"Mission accomplished. Return to base."},
    {27,6,"Return to base for a mission update."}, {28,6,"You've done all you can.",true}, {29,9,"and return to base."},
    {30,12,"they could be approaching!",false,9}, {31,11,"Mission complete- come back to base."}, {32,6,"Well done- you can return to base."},
    {33,4,"You can return to Hemmersan.",false,9}, {34,2,"Return to Hemmersan.",false,9},
    {35,1,nullptr,false,9}, {38,8,"Tolly: get back to base.",false,9},
    {39,4,"Return to base for a mission update.",false,9}, {40,6,"all targets are clear.",false,9},
    {41,7,"Return to Hemmersan.",false,9}, {42,6,"Return to base immediately, Tolly.",false,9},
    {43,8,"Return to Hemmersan, Tolly.",false,9}, {44,12,"Good job, Tolly. Return to base.",false,9},
    {48,12,"Mission accomplished. Return to base.",false,9}, {49,12,"and don't waste any time.",false,9},
    {52,3,"Return to Hemmersan.",false,9}, {53,5,"Mission complete- come back to base.",false,9},
    {57,7,"we advise you come back in.",false,9}, {59,15,"Mission accomplished. Return to base.",false,9},
    {60,6,"Return to Hemmersan, Tolly.",false,9}, {61,11,"Tolly: get back to base.",false,9},
    {62,11,"Mission accomplished. Return to base.",false,9}, {63,8,"Good job, Tolly. Return to base.",false,9},
    {64,9,"Mission accomplished. Return to base.",false,1,false},
    {70,6,"Radar clear; you're safe to return.",false,9}, {71,8,"all targets are clear.",false,9},
    {72,14,"Good job, Tolly. Return to base.",false,9},
    {78,3,"Return to base, Tolly.",false,9}, {79,10,"Well done- you can return to base.",false,9},
    {83,6,"Tolly: get back to base.",false,9}, {84,4,"Return to Hemmersan.",false,9}, {85,15,"and clear the network of enemy craft.",false,9},
  }};
  for(auto const &test : cases) {
    auto const mission{test.stage - 1};
    auto const &scenario{campaign.scenario(static_cast<uint8_t>(mission + 1))};
    auto const record_index{darker::resources::select_campaign_stage(static_cast<uint8_t>(mission + 1)).record};
    auto const &record{scenario.records()[record_index]};
    auto cells{darker::game::make_city_map(archives.load({0,68}),true)};
    darker::game::assign_city_variants(cells,limits);
    darker::game::apply_scenario_cells(cells,record);
    darker::game::mission_combat combat{darker::game::make_scenario_group(record.groups[0], bank, 1, 0, record.shared.offset)};
    combat.reserves = darker::game::make_scenario_group(record.groups[1],bank,static_cast<uint8_t>(1 + record.groups[0].objects.size()),0,record.shared.offset);
    combat.free_actors = darker::game::make_scenario_group(record.groups[2],bank,
      static_cast<uint8_t>(1 + record.groups[0].objects.size() + record.groups[1].objects.size()),0,record.shared.offset);
    darker::game::prepare_delphi_aircraft_sites(combat.spawning,cells);
    darker::game::player_flight player;
    auto &caero{std::get<darker::game::caero_flight_state>(player.craft)};
    caero.flying = true;
    caero.energy.reserve = 0xcfff;
    combat.primary_weapon = test.weapon <= 3 ? test.weapon : 0;
    combat.secondary_weapon = test.weapon > 3 ? test.weapon : 0;
    combat.difficulty = static_cast<uint8_t>((mission + 1)*2);
    if(test.stage == 48) {
      auto launchers{combat.actors};
      std::erase_if(launchers,[](auto const &actor){ return actor.definition_slot != 32; });
      if(launchers.size() != 6) throw std::runtime_error{"Mission 48 lost its six mobile launchers"};
      darker::game::mission_combat ground{std::move(launchers)};
      ground.difficulty = combat.difficulty;
      darker::game::player_flight target_player;
      bool fired{false};
      for(uint32_t clock{8}; clock < 100000 && !fired && !ground.actors.empty(); clock += 8) {
        auto const &launcher{ground.actors.front()};
        auto const heading{launcher.pose.angles[0]};
        auto const sine{darker::maths::original_sine[heading >> 6]};
        auto const cosine{darker::maths::original_sine[((heading >> 6)+256)%1024]};
        target_player.pose().position = {static_cast<uint16_t>(launcher.pose.position[0] + ((sine*700) >> 15)),
          static_cast<uint16_t>(launcher.pose.position[1] + ((cosine*700) >> 15)),1600};
        ground.advance(target_player,cells,bank,clock,8,0,false,scenario.bytes(record.shared),record.time_multiplier);
        fired = std::ranges::any_of(ground.hostile_projectiles.records(),[](auto const &shot){ return shot.parameters.definition == &darker::game::original_object_definitions[18]; });
      }
      if(!fired) throw std::runtime_error{"Mission 48 launcher routes never invoked their missile branch"};
    }
    darker::resources::font_resource const fonts{archives.load({.archive{0}, .slot{29}})};
    auto const text{scenario.language(record_index, darker::resources::scenario_language::english)};
    darker::presentation::player briefing{archives,fonts,scenario,record_index};
    do { briefing.advance(4000); } while(briefing.continue_page());
    auto const cursor{briefing.consumed_text()};
    darker::game::mission_context context{.program{scenario.bytes(record.shared)}, .text{text}, .cells{cells}, .time_multiplier{record.time_multiplier}, .text_cursor{cursor}};
    darker::game::mission_script script{.continuation{*record.player_program - record.shared.offset}};
    context.adjust_objectives = [&](uint8_t const operand){ combat.adjust_objectives(operand); return combat.remaining_objectives() == 0; };
    context.set_aircraft_spawning = [&](uint8_t const setting){ combat.spawning.enabled = setting != 0; };
    context.set_building_attacks = [&](uint8_t const setting){ combat.building_attacks = setting != 0; };
    darker::game::beacon_changes beacon_changes;
    context.change_beacons = [&](uint8_t const opcode, uint8_t const origin, uint8_t const count){
      beacon_changes.command(opcode,origin,count,static_cast<uint16_t>(context.clock),scenario.bytes(record.beacon_sequence));
    };
    context.activate_reserves = [&](uint8_t const opcode, uint8_t const count){
      combat.activate_reserves(static_cast<darker::game::actor_category>(opcode - 9),count,player.pose(),static_cast<uint16_t>(context.clock));
      return combat.remaining_objectives() == 0;
    };
    unsigned int shots{0};
    bool message{test.message == nullptr};
    bool saw_burst{false}, saw_trail{false};
    for(uint32_t clock{8}; clock < 300000; clock += 8) {
      auto const target{std::ranges::find_if(combat.actors, [&](auto const &actor){ return ((actor.attributes & 1) || test.stage == 84) && !(actor.flags & 0x20); })};
      bool const fire{target != combat.actors.end() && clock % 128 == 0};
      auto const weapon{target != combat.actors.end() && target->category != darker::game::actor_category::air ? uint8_t{1} : test.weapon};
      combat.primary_weapon = weapon <= 3 ? weapon : 0;
      if(combat.secondary_weapon != (weapon > 3 ? weapon : 0)) combat.target.clear();
      combat.secondary_weapon = weapon > 3 ? weapon : 0;
      if(target != combat.actors.end()) {
        player.pose().position = target->pose.position;
        player.pose().position[1] += 200;
        player.pose().position[2] += 92;
        if(target->category != darker::game::actor_category::air) player.pose().position[2] += bank.header_at(target->parameters.model_token).extent;
        player.pose().angles = {};
        player.pose().speed = 496;
        if(test.stage >= 33) {
          // Follow behind the target rather than launching across an Assassin's lateral motion.
          auto const heading{static_cast<uint16_t>(target->pose.angles[0]+(target->category != darker::game::actor_category::air ? 0x8000 : 0))};
          auto const sine{darker::maths::original_sine[heading >> 6]};
          auto const cosine{darker::maths::original_sine[((heading >> 6)+256)%1024]};
          player.pose().position[0] = static_cast<uint16_t>(target->pose.position[0] + ((sine*200) >> 15));
          player.pose().position[1] = static_cast<uint16_t>(target->pose.position[1] + ((cosine*200) >> 15));
          player.pose().angles[0] = heading;
          combat.targeting_basis = darker::maths::make_view_basis({.heading{heading}});
        }
      }
      beacon_changes.advance(cells,static_cast<uint16_t>(clock));
      combat.spawn_aircraft(player,cells,bank,static_cast<uint16_t>(clock),8);
      combat.advance(player, cells, bank, clock, 8, static_cast<uint16_t>(clock ^ (clock - 8)), weapon <= 3 && fire, scenario.bytes(record.shared),record.time_multiplier,nullptr,(weapon == 10 && fire) || (weapon == 9 && clock % 2048 == 8),weapon == 9 && clock % 2048 != 0);
      if(combat.player_fired) ++shots;
      saw_burst |= !combat.effects.emitters.empty();
      saw_trail |= !combat.effects.trails.empty();
      // Supply controlled beacon power while isolating aim/collision/completion from navigation.
      darker::game::charge_caero_energy(caero.energy, 13056, 1, 1028, false);
      context.clock = clock;
      context.object_flags = combat.status_flags(player.lifecycle.flags);
      context.objectives_complete = combat.remaining_objectives() == 0;
      context.object_counter = static_cast<uint8_t>(combat.completed_objectives);
      context.messages.clear();
      darker::game::advance_mission_script(script, context);
      for(auto const &event : context.messages) {
        if(mission < 3 && !context.objectives_complete) throw std::runtime_error{"Return message preceded objective completion"};
        std::string const actual{reinterpret_cast<char const *>(text.data() + event.offset), event.length};
        if(mission < 3 && actual != std::array{"Well done- you can return to base.","Mission accomplished. Return to base.","Good job, Tolly. Return to base."}[mission]) throw std::runtime_error{"Incorrect first-mission return message"};
        message |= test.message && actual == test.message;
      }
      if(message && script.stopped && combat.remaining_objectives() == 0) break;
    }
    auto const live_objectives{static_cast<unsigned>(std::ranges::count_if(combat.actors,[](auto const &actor){ return actor.attributes & 1; }))};
    bool const correct_removals{test.permits_survivors
      ? combat.completed_objectives >= test.removals && combat.completed_objectives + live_objectives == 11
      : combat.completed_objectives == test.removals};
    if(!message || !script.stopped || !correct_removals || combat.remaining_objectives() != 0 || player.lifecycle.crashing) {
      throw std::runtime_error{"Campaign controlled combat did not complete: mission=" + std::to_string(mission + 1) + ", shots=" + std::to_string(shots)
        + ", removed=" + std::to_string(combat.completed_objectives) + ", remaining=" + std::to_string(combat.remaining_objectives()) + ", reserves=" + std::to_string(combat.reserves.size())
        + ", stopped=" + std::to_string(script.stopped) + ", message=" + std::to_string(message) + ", crashing=" + std::to_string(player.lifecycle.crashing)
        + ", reserve=" + std::to_string(caero.energy.reserve)};
    }
    if(!saw_burst || (test.aircraft_trails && !saw_trail)) throw std::runtime_error{"Combat omitted hit bursts or damage trails"};
    auto const destination{briefing.departure_destination ? briefing.departure_destination : uint16_t{0x7162}};
    player.pose().position = {static_cast<uint16_t>((destination & 255)*128+128),static_cast<uint16_t>((destination & 0xff00)+152-700),500};
    player.pose().angles = {0x8000, 0, 0};
    caero.damage.rotation = {};
    darker::game::hangar_state hangar{.return_site{destination}};
    if(!darker::game::begin_hangar_return(player, cells, hangar, combat.remaining_objectives() == 0)) throw std::runtime_error{"Completed mission refused HQ return"};
    for(unsigned int frame{0}; frame < 2000 && hangar.returning != darker::game::hangar_return_phase::complete; ++frame) {
      context.clock += 8;
      darker::game::advance_hangar_return(player, hangar, 8, static_cast<uint16_t>(context.clock));
      darker::game::advance_hangar_departure(player, cells, hangar, 8);
    }
    if(hangar.returning != darker::game::hangar_return_phase::complete) throw std::runtime_error{"First mission did not finish docking"};
    std::cout << "Mission " << mission + 1 << " controlled combat: " << shots << " shots, " << combat.completed_objectives << " objectives removed, return message and completed HQ docking verified." << std::endl;
  }
  struct transfer_case { uint8_t stage; uint16_t origin, destination; size_t actors, messages; };
  for(auto const test : std::array<transfer_case,9>{{
    {16,0x7162,0x3064,4,0},{18,0x3064,0x7162,4,2},{23,0x7162,0x4c64,0,0},
    {37,0x3060,0x7162,0,0},{45,0x7162,0x0c84,7,0},{47,0x0c84,0x7162,4,0},{56,0x0d88,0x7162,8,4},{66,0x7162,0x4d62,8,0},{68,0x4d62,0x7162,10,3},
  }}) {
    auto const stage{test.stage};
    auto const &transfer{campaign.scenario(stage)};
    auto const record_index{darker::resources::select_campaign_stage(stage).record};
    auto const &record{transfer.records()[record_index]};
    auto const origin{test.origin}, destination{test.destination};
    darker::resources::font_resource const fonts{archives.load({0,29})};
    darker::presentation::player briefing{archives,fonts,transfer,record_index};
    do { briefing.advance(4000); } while(briefing.continue_page());
    darker::game::mission_combat traffic{darker::game::make_scenario_group(record.groups[0],bank,1,0,record.shared.offset)};
    if(briefing.departure_destination != destination || traffic.actors.size() != test.actors || traffic.remaining_objectives() != 0 || !record.groups[1].objects.empty()) {
      throw std::runtime_error{"Transfer mission did not select the original destination"};
    }
    auto transfer_cells{darker::game::make_city_map(archives.load({0,68}),true)};
    darker::game::player_flight player;
    darker::game::hangar_state hangar{.return_site{origin},.next_return_site{briefing.departure_destination}};
    darker::game::initialise_caero_hangar(player,transfer_cells,hangar,bank.header_at(bank.special_models()[25]).height);
    for(uint32_t clock{8}; clock <= 4000; clock += 8) traffic.advance(player,transfer_cells,bank,clock,8,0,false,transfer.bytes(record.shared));
    // Isolate the departure boundary and destination handoff from manual navigation.
    player.pose().position[1] -= 768;
    darker::game::advance_hangar_departure(player,transfer_cells,hangar,8);
    if(hangar.return_site != destination || (player.lifecycle.flags & 16)
      || transfer_cells[origin/2].state != 0 || transfer_cells[destination/2].state != 0) {
      throw std::runtime_error{"Hangar departure did not close the old site before changing destination"};
    }
    darker::game::mission_script script{.continuation{*record.player_program - record.shared.offset}};
    darker::game::mission_context context{.program{transfer.bytes(record.shared)},
      .text{transfer.language(record_index,darker::resources::scenario_language::english)},.cells{transfer_cells},
      .time_multiplier{record.time_multiplier},.objectives_complete{true},.text_cursor{briefing.consumed_text()}};
    for(uint32_t clock{0}; clock < 30000 && !script.stopped; clock += 8) {
      context.clock = clock;
      darker::game::advance_mission_script(script,context);
    }
    if(!script.stopped || context.messages.size() != test.messages) throw std::runtime_error{"Transfer mission omitted its timed messages or stop"};
    player.pose().position = {static_cast<uint16_t>((destination & 255)*128+128),static_cast<uint16_t>((destination & 0xff00)+152-700),500};
    player.pose().angles = {0x8000,0,0};
    if(!darker::game::begin_hangar_return(player,transfer_cells,hangar,true)) throw std::runtime_error{"Transfer mission refused its destination hangar"};
    for(unsigned int frame{0}; frame < 2000 && hangar.returning != darker::game::hangar_return_phase::complete; ++frame) {
      darker::game::advance_hangar_return(player,hangar,8,static_cast<uint16_t>(frame*8));
      darker::game::advance_hangar_departure(player,transfer_cells,hangar,8);
    }
    if(hangar.returning != darker::game::hangar_return_phase::complete || hangar.return_site != destination) {
      throw std::runtime_error{"Transfer mission did not complete at the destination hangar"};
    }
    std::cout << "Mission " << static_cast<unsigned int>(stage) << ": briefing destination, timed messages, departure handoff and docking verified." << std::endl;
  }
  {
    auto const &source{campaign.scenario(20)};
    auto const &record{source.records()[3]};
    auto city{darker::game::make_city_map(archives.load({0,68}),true)};
    darker::game::assign_city_variants(city,limits);
    darker::game::apply_scenario_cells(city,record);
    darker::game::mission_combat combat{{}};
    combat.free_actors = darker::game::make_scenario_group(record.groups[2],bank,16,0,record.shared.offset);
    darker::game::prepare_delphi_aircraft_sites(combat.spawning,city);
    darker::game::player_flight player;
    player.pose().position = {21632,14584,2000};
    combat.spawn_aircraft(player,city,bank,8,8);
    if(combat.actors.size() != 1 || combat.free_actors.size() != 3) throw std::runtime_error{"Occupied mission-twenty warehouse did not launch an aircraft"};
    auto const identity{combat.actors.front().index};
    bool departed{false};
    for(uint32_t clock{8}; clock < 5000 && !combat.actors.empty(); clock += 8) {
      if(clock >= 1100) player.pose().position = {};
      combat.advance(player,city,bank,clock,8,0,false,source.bytes(record.shared),record.time_multiplier);
      if(!combat.actors.empty() && combat.actors.front().parameters.update_entry == 0x8823) departed = true;
    }
    if(!departed || !combat.actors.empty() || combat.free_actors.size() != 4 || combat.completed_objectives != 0) {
      throw std::runtime_error{"Warehouse aircraft failed to depart, retire at distance and return to the free list"};
    }
    player.pose().position = {21632,14584,2000};
    combat.spawning.timers.fill(0);
    combat.spawn_aircraft(player,city,bank,5000,8);
    if(combat.actors.size() != 1 || combat.actors.front().index != identity) throw std::runtime_error{"Warehouse did not reuse the retired aircraft's native identity"};
    std::cout << "Mission twenty warehouse: launch, protected take-off, ordinary AI, distance retirement and reuse verified." << std::endl;
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
  {
    // Retiring a reusable aircraft must break missile references before that identity can launch again.
    auto actor{darker::game::make_scenario_group(scenario.records()[0].groups[0],bank,1,0,scenario.records()[0].shared.offset).front()};
    actor.flags = 0x28;
    actor.expiry = 0;
    actor.attributes = 0xfe;
    darker::game::mission_combat combat{{actor}};
    darker::game::player_flight player;
    player.pose().position = {10000,10000,10000};
    darker::game::city_map city{};
    auto const token{static_cast<uint16_t>(0xd986 + actor.index*112)};
    darker::game::launch_emitter const emitter{.position{10000,12000,10000},.definition_strength{40}};
    auto *shot{combat.projectiles.launch({.definition{darker::game::original_object_definitions[9]},.emitter{emitter},
      .model_token{bank.special_models()[9]},.lifetime{2000},.target_token{token}})};
    combat.target.token = token;
    combat.advance(player,city,bank,8,8,0,false);
    if(shot->target_token != shot->native_id || !combat.actors.empty() || combat.free_actors.size() != 1 || combat.target.token != 0xffff
      || !(combat.status_flags(player.lifecycle.flags)[actor.index] & 0x20))
      throw std::runtime_error{"Retired aircraft retained a Hunter or selected-target reference"};
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
      std::array<int,13> const actual{player.pose().position[0],player.pose().position[1],player.pose().position[2],
        player.pose().angles[0],player.pose().angles[1],player.lifecycle.flags,player.forward_setting,
        craft.energy.reserve,craft.energy.boost,player.tunnel->connection.cell,
        player.tunnel->lookahead,player.tunnel->resistance,player.tunnel->off_route_time};
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
    for(uint16_t const step : std::array<uint16_t,2>{8,10}) {
      std::span<std::array<uint16_t,30> const> const samples{step == 8
        ? std::span<std::array<uint16_t,30> const>{darker::test_reference::tunnel_launch_8_samples}
        : std::span<std::array<uint16_t,30> const>{darker::test_reference::tunnel_launch_10_samples}};
      darker::game::player_flight player;
      darker::game::initialise_tunnel_entry(player,0x3064,128,underground_bank.header_at(underground_bank.special_models()[28]).height);
      auto city{*maps[0]};
      darker::game::hangar_state portal{.return_site{0x3064},.next_return_site{0x3064}};
      uint16_t clock{0};
      for(auto const &expected : samples) {
        clock += step;
        player.advance({},false,step,clock,underground_bank,city,&network);
        darker::game::update_tunnel_portal(player,city,portal,step);
        auto const &craft{std::get<darker::game::caero_flight_state>(player.craft)};
        auto const &flight{*player.tunnel};
        std::array<uint16_t,30> const actual{player.pose().position[0],player.pose().position[1],player.pose().position[2],
          player.pose().fractions[0],player.pose().fractions[1],player.pose().fractions[2],
          player.pose().angles[0],player.pose().angles[1],player.pose().angles[2],player.pose().speed,
          craft.horizontal_velocity,craft.vertical_velocity,flight.heading_rate,craft.damage.rotation.pitch,craft.damage.rotation.turn,
          flight.connection.cell,flight.progress,flight.connection.route,flight.filtered_pitch,flight.filtered_bank,
          flight.off_route_time,flight.resistance,flight.lookahead,player.forward_setting,player.lifecycle.flags,
          static_cast<uint16_t>(player.lifecycle.crashing),craft.energy.reserve,craft.energy.boost,craft.energy.incoming_display,craft.energy.reserve_display};
        for(size_t field{0}; field < actual.size(); ++field) if(actual[field] != expected[field]) {
          throw std::runtime_error{"Tunnel launch differs from native: clock=" + std::to_string(clock)
            + ", field=" + std::to_string(field) + ", actual=" + std::to_string(actual[field]) + ", expected=" + std::to_string(expected[field])};
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
    struct tunnel_case { uint8_t stage; uint16_t entry; unsigned int reserves, objectives; };
    for(auto const test : std::array<tunnel_case,7>{{{17,0x3064,13,16},{24,0x3f64,14,20},{36,0x3060,15,19},{46,0x1784,10,14},{55,0x1088,18,23},{67,0x4362,24,36},{86,0x693a,10,18}}}) {
      auto const &scenario{campaign.scenario(test.stage)};
      auto const record_index{darker::resources::select_campaign_stage(test.stage).record};
      auto const &underground_record{scenario.records()[record_index]};
      auto city{darker::game::make_city_map(archives.load({0,70u+(underground_record.configuration >> 4)}),false)};
      darker::game::mission_combat combat{darker::game::make_scenario_group(underground_record.groups[0],underground_bank,1,2,
        underground_record.shared.offset,darker::game::tunnel_setup{network,city})};
      combat.reserves = darker::game::make_scenario_group(underground_record.groups[1],underground_bank,static_cast<uint8_t>(1 + underground_record.groups[0].objects.size()),2,
        underground_record.shared.offset,darker::game::tunnel_setup{network,city});
      darker::resources::font_resource const fonts{archives.load({0,29})};
      darker::presentation::player briefing{archives,fonts,scenario,record_index};
      do { briefing.advance(4000); } while(briefing.continue_page());
      if(!briefing.entry || briefing.entry->site != test.entry || briefing.entry->heading != 128) throw std::runtime_error{"Tunnel briefing lost its entry placement"};
      darker::game::player_flight player;
      darker::game::initialise_tunnel_entry(player,briefing.entry->site,briefing.entry->heading,underground_bank.header_at(underground_bank.special_models()[28]).height);
      player.lifecycle.flags = 0;
      auto &craft{std::get<darker::game::caero_flight_state>(player.craft)};
      darker::game::mission_context context{.program{scenario.bytes(underground_record.shared)},
        .text{scenario.language(record_index,darker::resources::scenario_language::english)},.cells{city},
        .time_multiplier{underground_record.time_multiplier},.text_cursor{briefing.consumed_text()}};
      context.select_weapon = [&](uint8_t const selection){ combat.primary_weapon = selection; };
      unsigned int admitted{0};
      context.activate_reserves = [&](uint8_t const opcode, uint8_t const count){
        auto const before{combat.reserves};
        combat.activate_reserves(static_cast<darker::game::actor_category>(opcode-9),count,player.pose(),static_cast<uint16_t>(context.clock));
        for(auto const &actor : combat.actors) {
          auto const original{std::ranges::find_if(before,[&](auto const &item){ return item.index == actor.index; })};
          if(original != before.end() && (actor.pose.position != original->pose.position || actor.tunnel.has_value() != original->tunnel.has_value()
            || (actor.tunnel && actor.tunnel->route != original->tunnel->route))) {
            throw std::runtime_error{"Underground reserves were moved away from their routes"};
          }
        }
        admitted += count;
        return combat.remaining_objectives() == 0;
      };
      if(test.stage == 36) {
        std::vector<darker::game::scenario_actor> stationary;
        for(auto const &actor : combat.reserves) if(actor.category == darker::game::actor_category::stationary) stationary.push_back(actor);
        if(stationary.size() != 2) throw std::runtime_error{"Communications tunnel lost its two stationary aircraft"};
        // Force a terrain intersection: static records are targets, never moving collision owners in 6F0F.
        stationary.front().pose.position = {};
        stationary.front().previous_position = {};
        darker::game::mission_combat idle{stationary};
        for(uint32_t time{8}; time < 1024; time += 8) idle.advance(player,city,underground_bank,time,8,0,false,{},50,&network);
        if(idle.actors.size() != stationary.size() || idle.completed_objectives != 0) throw std::runtime_error{"Stationary tunnel objects expired without an impact"};
        for(size_t i{0}; i < stationary.size(); ++i) {
          if(idle.actors[i].pose.position != stationary[i].pose.position || idle.actors[i].flags != stationary[i].flags
            || idle.actors[i].parameters.update_entry != 0) throw std::runtime_error{"Stationary aircraft entered the moving-object collision pass"};
        }
      }
      darker::game::mission_script script{.continuation{*underground_record.player_program - underground_record.shared.offset}};
      unsigned int shots{0};
      uint32_t clock{0};
      for(clock = 8; clock < 500000; clock += 8) {
        auto const target{std::ranges::find_if(combat.actors,[](auto const &actor){ return (actor.attributes & 1) && !(actor.flags & 0x20); })};
        bool const fire{target != combat.actors.end() && clock % 128 == 0};
        if(target != combat.actors.end()) {
          player.pose().position = target->pose.position;
          player.pose().position[1] += 100;
          player.pose().position[2] += 92;
          player.pose().angles = {};
          player.pose().speed = 496;
        }
        craft.energy.reserve = 0xcfff;
        combat.advance(player,city,underground_bank,clock,8,static_cast<uint16_t>(clock ^ (clock-8)),fire,
          scenario.bytes(underground_record.shared),underground_record.time_multiplier,&network);
        shots += combat.player_fired;
        context.clock = clock;
        context.objectives_complete = combat.remaining_objectives() == 0;
        context.object_counter = static_cast<uint8_t>(combat.completed_objectives);
        darker::game::advance_mission_script(script,context);
        if(script.stopped && combat.remaining_objectives() == 0) break;
      }
      if(clock >= 500000 || admitted != test.reserves || combat.completed_objectives != test.objectives || player.lifecycle.crashing) {
        throw std::runtime_error{"Tunnel combat did not complete: stage=" + std::to_string(test.stage) + ", time=" + std::to_string(clock) + ", admitted=" + std::to_string(admitted)
          + ", removed=" + std::to_string(combat.completed_objectives) + ", remaining=" + std::to_string(combat.remaining_objectives())
          + ", shots=" + std::to_string(shots)};
      }
      player.pose() = {.position{static_cast<uint16_t>((test.entry & 255)*128 + 128),static_cast<uint16_t>((test.entry & 0xff00)+242),800}};
      player.tunnel->connection.route = 128;
      player.forward_setting = 130;
      craft.damage.rotation = {};
      darker::game::hangar_state portal{.return_site{test.entry}};
      darker::game::update_tunnel_portal(player,city,portal,8);
      for(unsigned int frame{0}; frame < 2000 && portal.returning != darker::game::hangar_return_phase::complete; ++frame) {
        clock += 8;
        darker::game::advance_hangar_return(player,portal,8,static_cast<uint16_t>(clock));
      }
      if(portal.returning != darker::game::hangar_return_phase::complete) throw std::runtime_error{"Completed tunnel refused portal return"};
      std::cout << "Mission " << static_cast<unsigned>(test.stage) << ": Wrecker door changes, " << admitted << " reinforcements, "
        << combat.completed_objectives << " objective removals and portal return verified ("
        << shots << " controlled shots)." << std::endl;
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
  {
    // Supply the failed power-station state while retaining the original enemy script and its timed patrol instructions.
    auto const &source{campaign.scenario(27)};
    auto const &record{source.records()[2]};
    auto city{darker::game::make_city_map(archives.load({0,68}),true)};
    for(auto &cell : city) if(cell.type != 1) cell.state |= 0x20;
    auto actors{darker::game::make_scenario_group(record.groups[0],bank,1,0,record.shared.offset)};
    uint16_t owner{0};
    uint32_t clock{0};
    for(clock = 16; clock < 250000 && !owner; clock += 16) {
      for(auto &actor : actors) {
        darker::game::mission_context context{.program{source.bytes(record.shared)},.cells{city},.clock{clock},.time_multiplier{record.time_multiplier}};
        context.set_target = [&](uint16_t token, bool flagged){ actor.target_token = token; actor.flags = flagged ? 2 : 0; };
        context.register_owner = [&]{ return std::exchange(owner,static_cast<uint16_t>(0xd986 + actor.index*112)); };
        darker::game::advance_mission_script(actor.script,context);
      }
    }
    if(!owner) throw std::runtime_error{"Destroyed power station never registered its blackout owner"};
    clock -= 16;
    auto const &supplementary{campaign.supplementary()};
    auto const &blackout{supplementary.records()[7]};
    darker::game::mission_exchange exchange{.alternate{darker::game::mission_context_slot{supplementary.bytes(blackout.shared),
      supplementary.language(7,darker::resources::scenario_language::english),blackout.entry_offset - blackout.shared.offset}}};
    darker::game::mission_script script{.continuation{*record.player_program - record.shared.offset}};
    darker::game::mission_context context{.program{source.bytes(record.shared)},.text{source.language(2,darker::resources::scenario_language::english)},
      .cells{city},.clock{clock},.time_multiplier{record.time_multiplier}};
    darker::game::beacon_changes fade;
    context.change_beacons = [&](uint8_t op, uint8_t origin, uint8_t count){ fade.command(op,origin,count,static_cast<uint16_t>(context.clock),{}); };
    exchange.exchange(script,context,std::nullopt);
    if(!exchange.supplementary_active || context.text.data() != supplementary.language(7,darker::resources::scenario_language::english).data()
      || script.deadline != static_cast<uint16_t>(clock) || exchange.alternate->continuation)
      throw std::runtime_error{"Late-frame blackout entry did not install its original message/script context"};
    unsigned int messages{0};
    auto const entered{clock};
    for(clock += 16; clock < entered + 40000; clock += 16) {
      context.clock = clock;
      fade.advance(city,static_cast<uint16_t>(clock));
      context.messages.clear();
      darker::game::advance_mission_script(script,context);
      messages += static_cast<unsigned int>(context.messages.size());
    }
    auto const dark{std::ranges::count_if(city,[](auto cell){ return cell.type == 1 && cell.state == 0; })};
    if(messages != 9 || dark != 224 || script.stopped || !exchange.supplementary_active)
      throw std::runtime_error{"Power-station failure did not enter the persistent shared blackout sequence"};
    std::cout << "Mission 27 power failure: actual enemy scripts register the owner, switch message context and extinguish 224 towers." << std::endl;
  }
  std::cout << "Fourth-mission flatbed traversed its original route and removed itself after 155,950 ticks at 50-tick sampling." << std::endl;

}
