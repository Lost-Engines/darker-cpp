#include "skimma_combat_check.h"
#include <algorithm>
#include <array>
#include <bit>
#include <iostream>
#include <stdexcept>
#include <utility>
#include "game/mission_combat.h"
#include "game/mission_exchange.h"
#include "game/object_definitions.h"
#include "game/projectile_steering.h"
#include "game/scenario_setup.h"
#include "game/scenario_world.h"
#include "maths/direction.h"
#include "maths/sine_table.h"
#include "presentation/player.h"
#include "reference/halon_spawning_samples.h"
#include "resources/archive_set.h"
#include "resources/campaign.h"

void check_skimma_combat(darker::resources::archive_set const &archives) {
  /// Exercise original Skimma combat, Halon objectives, supply exchanges and the final battle
  darker::resources::geometry_bank const bank{archives.load({0,31})};
  for(bool const upgraded : {false, true}) {
    // Exercise the connected keyboard path: native departure tests D5CA, not the timestep-scaled drive.
    darker::game::player_flight pilot;
    darker::game::initialise_skimma_pad(pilot, 0x2020, 0, 0, upgraded);
    pilot.supply.phase = darker::game::supply_phase::docked;
    darker::game::city_map city{};
    for(unsigned int frame{0}; frame < 300; ++frame) pilot.advance_motion({}, false, 8, bank, city);
    if(pilot.supply.phase != darker::game::supply_phase::docked)
      throw std::runtime_error{"Skimma left the supply pad without pitch input"};
    for(unsigned int frame{0}; frame < 2; ++frame) pilot.advance_motion({.down{true}}, false, 8, bank, city);
    if(pilot.supply.phase != darker::game::supply_phase::flight || (pilot.lifecycle.flags & 0x10)
      || std::get<darker::game::skimma_flight_state>(pilot.craft).vertical_velocity != 200)
      throw std::runtime_error{"Down-arrow input failed to release the Skimma from its supply pad"};
  }
  for(auto const &v : darker::test_reference::halon_spawning_samples) {
    darker::game::aircraft_spawning state{.sites{0x0969,0x0b6f,0x0d6f},.departure_heading{static_cast<uint16_t>(v[10])},.halon{true},.enabled{v[5] != 0}};
    state.timers[0] = static_cast<uint16_t>(v[6]);
    darker::game::city_map city{};
    city[(v[0] >> 8)*128+(v[0] & 127)] = {76,static_cast<uint8_t>(v[1])};
    darker::game::object_pose const player{.position{static_cast<uint16_t>(v[2]*256),static_cast<uint16_t>(v[3]*256),0}};
    std::vector<darker::game::scenario_actor> active, free;
    if(v[4]) {
      free.emplace_back();
      free.back().script.checkpoint = 0xe800;
      darker::game::apply_object_definition(free.back().parameters,darker::game::original_object_definitions[20],bank.special_models()[20]);
    }
    auto random{static_cast<uint16_t>(v[9])};
    darker::game::advance_aircraft_spawning(state,active,free,city,bank,player,static_cast<uint16_t>(v[7]),static_cast<uint16_t>(v[8]),random);
    if(active.size() != static_cast<size_t>(v[11]) || state.timers[0] != v[12] || random != v[13] || state.departure_heading != v[14])
      throw std::runtime_error{"Halon aircraft admission, timer or heading differs from the original"};
    if(active.empty()) continue;
    auto const &actor{active.front()};
    std::array<uint16_t,15> const actual{actor.pose.position[0],actor.pose.position[1],actor.pose.position[2],
      actor.pose.angles.heading,actor.pose.angles.pitch,actor.pose.angles.roll,actor.pose.speed,actor.selected_target,actor.target_token,
      actor.current_cell,actor.parameters.update_entry,actor.expiry,actor.script.deadline,actor.flags,
      static_cast<uint16_t>(actor.script.continuation == 0xe800)};
    for(size_t i{0}; i < actual.size(); ++i) if(actual[i] != v[i+15])
      throw std::runtime_error{"Halon aircraft placement differs from the original: field="+std::to_string(i)};
  }
  darker::resources::campaign_resources campaign{archives};
  auto const &scenario{campaign.scenario(103)};
  auto const &record{scenario.records()[6]};
  auto cells{darker::game::make_city_map(archives.load({0,69}),false)};
  unsigned int pads{0};
  for(size_t cell{0}; cell < cells.size(); ++cell) {
    darker::game::player_flight candidate;
    candidate.craft = darker::game::skimma_flight_state{.pose{.position{static_cast<uint16_t>((cell%128)*256+128),
      static_cast<uint16_t>((cell/128)*256+255),512}},.horizontal_velocity{100},.vertical_velocity{0xffff}};
    candidate.engine_flags = 0;
    bool const accepted{darker::game::begin_supply_approach(candidate,cells,candidate.supply)};
    if(accepted != (cells[cell].type == 3)) throw std::runtime_error{"Supply entry differs from the native all-cell Halon map check"};
    if(accepted) {
      ++pads;
      if(candidate.supply.site != (cell/128)*256+(cell%128)*2 || candidate.supply.offset != 0x8080)
        throw std::runtime_error{"Supply target does not match the original map cell"};
    }
  }
  if(pads != 8) throw std::runtime_error{"Halon map no longer contains the eight original supply pads"};
  std::array<uint8_t,256> limits{};
  for(size_t i{0}; i < bank.city_types().size(); ++i) limits[i+1] = bank.city_types()[i].variant_limit;
  darker::game::assign_city_variants(cells,limits);
  darker::game::apply_scenario_cells(cells,record);
  for(int const weapon : {-1,0,2}) {
    auto initial{darker::game::make_scenario_group(record.groups[0],bank,1,1,record.shared.offset)};
    if(initial.size() != 4) throw std::runtime_error{"Skimma gun fixture lost its four original Halon craft"};
    darker::game::mission_combat combat{std::move(initial)};
    darker::game::player_flight player;
    player.craft = darker::game::skimma_flight_state{.damage{.shield_charge{0xbfff},.shield_enabled{true}}};
    player.engine_flags = 1;
    player.upgraded = weapon == 2;
    if(weapon >= 0) {
      combat.skimma_selection = static_cast<uint8_t>(weapon);
      auto &slot{combat.skimma_weapons[weapon]};
      darker::game::refill_skimma_weapon(slot.ammunition,static_cast<uint8_t>(weapon));
      slot.flags = 1;
    }
    auto &craft{std::get<darker::game::skimma_flight_state>(player.craft)};
    unsigned int shots{0};
    bool effects{false};
    for(uint32_t clock{8}; clock < 100000 && !combat.actors.empty(); clock += 8) {
      auto const target{std::ranges::find_if(combat.actors,[](auto const &actor){ return !(actor.flags & 0x20); })};
      if(target != combat.actors.end()) {
        auto const heading{target->pose.angles.heading};
        auto const sine{darker::maths::original_sine[heading >> 6]};
        auto const cosine{darker::maths::original_sine[((heading >> 6)+256)%1024]};
        player.pose().position = {static_cast<uint16_t>(target->pose.position[0]+((sine*200) >> 15)),
          static_cast<uint16_t>(target->pose.position[1]+((cosine*200) >> 15)),target->pose.position[2]};
        player.pose().angles = {heading,0,0};
        player.pose().speed = 496;
      }
      combat.targeting_basis = darker::maths::make_view_basis({player.pose().angles.heading,player.pose().angles.pitch,player.pose().angles.roll});
      bool const pressed{target != combat.actors.end() && clock%128 == 0};
      combat.advance(player,cells,bank,clock,8,static_cast<uint16_t>(clock^(clock-8)),weapon < 0 && pressed,
        scenario.bytes(record.shared),record.time_multiplier,nullptr,weapon >= 0 && pressed);
      if(weapon >= 0) darker::game::update_weapon_ring(combat.skimma_weapons[weapon].ammunition,combat.skimma_ring,
        static_cast<uint16_t>(clock),combat.skimma_weapons[weapon].flags,8);
      shots += combat.player_fired;
      effects |= !combat.effects.gun_sounds.empty();
      darker::game::recharge_skimma_shield(craft.damage,8);
      if(player.lifecycle.crashing) throw std::runtime_error{"Controlled Halon gun run lost the shielded pilot"};
    }
    if(!combat.actors.empty() || !shots || (weapon < 0 && !effects)) throw std::runtime_error{"Skimma primary gun did not clear the controlled Halon actors"};
    std::cout << "Skimma weapon " << weapon << ": four Halon craft cleared with " << shots << " controlled shots." << std::endl;
  }
  for(bool const shield : {false,true}) {
    darker::game::mission_combat incoming{{}};
    darker::game::player_flight victim;
    victim.craft = darker::game::skimma_flight_state{.pose{.position{14000,14000,4000}},
      .damage{.shield_charge{0xbfff},.shield_enabled{shield}}};
    darker::game::launch_emitter const emitter{.position{victim.pose().position},.definition_strength{40}};
    auto *shot{incoming.hostile_projectiles.launch({.definition{darker::game::original_object_definitions[18]},.emitter{emitter},
      .model_token{bank.special_models()[18]},.lifetime{4096},.target_token{0xd986}})};
    shot->placement.position = victim.pose().position;
    darker::game::city_map empty{};
    incoming.advance(victim,empty,bank,8,8,0,false);
    auto const &damage{std::get<darker::game::skimma_flight_state>(victim.craft).damage};
    if(!incoming.player_hit || victim.lifecycle.crashing == shield || (shield && damage.shield_charge >= 0xbfff))
      throw std::runtime_error{"Skimma projectile impact did not honour its enabled shield"};
  }

  for(size_t const record_index : {5u,6u}) {
    auto const &source{campaign.supplementary()};
    auto const &record{source.records()[record_index]};
    darker::game::player_flight pilot;
    pilot.craft = darker::game::skimma_flight_state{.pose{.position{0x1000,0x2000,400}},.damage{.shield_charge{0x4000}}};
    pilot.upgraded = record_index == 6;
    std::array<darker::game::skimma_weapon_slot,3> weapons{};
    darker::game::weapon_ring_state ring;
    uint8_t selected{0};
    uint16_t available{0};
    std::array<std::byte,1> const primary{std::byte{0x23}};
    darker::game::mission_script script;
    darker::game::mission_context context{.program{primary},.time_multiplier{50},.text_cursor{1}};
    context.transition_output = 0;
    darker::game::mission_exchange exchange{.alternate{darker::game::mission_context_slot{source.bytes(record.shared),
      source.language(record_index,darker::resources::scenario_language::english),record.entry_offset-record.shared.offset}}};
    context.select_weapon = [&](uint8_t const selection){
      available = std::rotl(uint16_t{0x8000},selection);
      darker::game::select_skimma_weapon(std::span{weapons}.first(pilot.upgraded ? 3 : 2),selected,ring,selection,available,static_cast<uint16_t>(context.clock));
    };
    context.refill_weapon = [&]{ darker::game::refill_skimma_weapon(weapons[selected].ammunition,selected); };
    context.reset_shield = [&]{ auto &charge{std::get<darker::game::skimma_flight_state>(pilot.craft).damage.shield_charge}; charge = static_cast<uint16_t>((charge & 255) | 0xbf00); };
    context.toggle_weapons = [&](uint16_t const mask){ available ^= mask; };
    context.exchange_context = [&](auto &active){ exchange.exchange(active,context,active.continuation); };
    for(unsigned int visit{0}; visit < 2; ++visit) {
      pilot.pose() = {.position{0x1000,0x2000,400}};
      pilot.lifecycle.flags = 0x10;
      pilot.supply = {.phase{darker::game::supply_phase::approach},.site{0x2020},.offset{0x1080}};
      std::get<darker::game::skimma_flight_state>(pilot.craft).horizontal_velocity = 0;
      context.clock = 0;
      context.transition_output = 0;
      exchange.enter_supply(script,context);
      unsigned int messages{0};
      uint32_t tick{0};
      for(; tick < 100000; tick += 16) {
        darker::game::advance_supply_motion(pilot,pilot.supply,context.transition_output,exchange.supplementary_active,
          exchange.supplementary_active ? 0 : 0x0c00,16);
        if(pilot.supply.phase == darker::game::supply_phase::flight) break;
        context.clock = tick;
        context.messages.clear();
        darker::game::advance_mission_script(script,context);
        messages += static_cast<unsigned int>(context.messages.size());
      }
      if(tick != (record_index == 5 ? 22176u : 26416u) || messages != (record_index == 5 ? 0u : 21u)
        || exchange.supplementary_active || context.text_cursor != 1)
        throw std::runtime_error{"Supply script/movement cycle differs from the original: record="+std::to_string(record_index)
          +", visit="+std::to_string(visit)+", tick="+std::to_string(tick)+", messages="+std::to_string(messages)};
      for(size_t i{0}; i < (pilot.upgraded ? 3u : 2u); ++i) {
        darker::game::weapon_ammunition expected;
        darker::game::refill_skimma_weapon(expected,static_cast<uint8_t>(i));
        if(weapons[i].ammunition.working != expected.working || weapons[i].ammunition.reserve != expected.reserve)
          throw std::runtime_error{"Supply cycle did not refill an available weapon"};
      }
      if(std::get<darker::game::skimma_flight_state>(pilot.craft).damage.shield_charge != 0xbf00)
        throw std::runtime_error{"Supply cycle did not restore the shield"};
    }
  }

  {
    darker::resources::geometry_bank const final_bank{archives.load({0,30})};
    darker::resources::font_resource const font{archives.load({0,29})};
    auto const &final_scenario{campaign.scenario(115)};
    auto const index{darker::resources::select_campaign_stage(115).record};
    auto const &record{final_scenario.records()[index]};
    auto city{darker::game::make_city_map(archives.load({0,68}),true)};
    std::array<uint8_t,256> variants{};
    for(size_t i{0}; i < final_bank.city_types().size(); ++i) variants[i+1] = final_bank.city_types()[i].variant_limit;
    darker::game::assign_city_variants(city,variants);
    darker::game::apply_scenario_cells(city,record);
    darker::presentation::player briefing{archives,font,final_scenario,index};
    do { briefing.advance(4000); } while(briefing.continue_page());
    darker::game::player_flight pilot;
    darker::game::initialise_skimma_pad(pilot,briefing.entry->site,briefing.entry->heading,
      final_bank.header_at(final_bank.special_models()[24]).height,true);
    pilot.scenario_configuration = 0;
    pilot.supply.phase = darker::game::supply_phase::flight;
    darker::game::weapon_ammunition second_weapon;
    darker::game::refill_skimma_weapon(second_weapon,1);
    auto groups{darker::game::make_scenario_actors(record,final_scenario,final_bank,pilot,second_weapon,0)};
    if(second_weapon.working || second_weapon.reserve || pilot.definition_slot() != 24 || pilot.world_damage_mask() != 0x20)
      throw std::runtime_error{"Final approach lost its native Skimma profile or empty ground-weapon bay"};
    darker::game::mission_combat combat{std::move(groups[0])};
    combat.reserves = std::move(groups[1]);
    combat.skimma_selection = 2;
    combat.skimma_weapons[2].flags = 1;
    darker::game::refill_skimma_weapon(combat.skimma_weapons[2].ammunition,2);
    darker::game::refill_skimma_weapon(combat.skimma_weapons[0].ammunition,0);
    combat.free_actors = std::move(groups[2]);
    auto &craft{std::get<darker::game::skimma_flight_state>(pilot.craft)};
    craft.damage.shield_enabled = true;
    pilot.engine_flags = 1;
    darker::game::world_objectives objectives{.list{record.objective_cell_list}};
    darker::game::mission_script script{.continuation{*record.player_program-record.shared.offset}};
    darker::game::mission_context context{.program{final_scenario.bytes(record.shared)},
      .text{final_scenario.language(index,darker::resources::scenario_language::english)},.cells{city},
      .time_multiplier{record.time_multiplier},.text_cursor{briefing.consumed_text()}};
    context.activate_reserves = [&](uint8_t const opcode,uint8_t const count){
      combat.activate_reserves(static_cast<darker::game::actor_category>(opcode-9),count,pilot.pose(),static_cast<uint16_t>(context.clock));
      return objectives.complete(record) && !combat.remaining_objectives();
    };
    unsigned int shots{0};
    for(uint32_t clock{8}; clock < 500000 && !context.progress; clock += 8) {
      auto const target{std::ranges::find_if(combat.actors,[](auto const &actor){ return !(actor.flags & 0x20); })};
      if(target != combat.actors.end()) {
        auto const heading{target->pose.angles.heading};
        auto const sine{darker::maths::original_sine[heading >> 6]};
        auto const cosine{darker::maths::original_sine[((heading >> 6)+256)%1024]};
        pilot.pose().position = {static_cast<uint16_t>(target->pose.position[0]+((sine*200) >> 15)),
          static_cast<uint16_t>(target->pose.position[1]+((cosine*200) >> 15)),target->pose.position[2]};
        pilot.pose().angles = {heading,0,0};
        pilot.pose().speed = 496;
      }
      combat.targeting_basis = darker::maths::make_view_basis({pilot.pose().angles.heading,pilot.pose().angles.pitch,0});
      if(combat.skimma_selection == 2 && !combat.skimma_weapons[2].ammunition.working && !combat.skimma_weapons[2].ammunition.reserve) {
        darker::game::select_skimma_weapon(combat.skimma_weapons,combat.skimma_selection,combat.skimma_ring,1,7,static_cast<uint16_t>(clock));
        combat.target.clear();
      }
      // This fixture controls the firing position and shield reserve; hostile damage is checked separately above.
      craft.damage.shield_charge = 0xbf00;
      bool const heavy{target != combat.actors.end() && target->parameters.definition->impact_strength >= 50};
      bool const pressed{target != combat.actors.end() && clock%128 == 0};
      combat.advance(pilot,city,final_bank,clock,8,static_cast<uint16_t>(clock^(clock-8)),
        pressed && !heavy,final_scenario.bytes(record.shared),record.time_multiplier,nullptr,pressed && heavy);
      darker::game::update_weapon_ring(combat.skimma_weapons[combat.skimma_selection].ammunition,combat.skimma_ring,static_cast<uint16_t>(clock),
        combat.skimma_weapons[combat.skimma_selection].flags,8);
      shots += combat.player_fired;
      darker::game::recharge_skimma_shield(craft.damage,8);
      if(pilot.lifecycle.crashing) throw std::runtime_error{"Controlled final battle lost its shielded pilot"};
      objectives.advance(city,record,0x20);
      context.clock = clock;
      context.objectives_complete = objectives.complete(record) && !combat.remaining_objectives();
      context.object_counter = static_cast<uint8_t>(combat.completed_objectives);
      context.object_flags = combat.status_flags(pilot.lifecycle.flags);
      context.messages.clear();
      darker::game::advance_mission_script(script,context);
    }
    if(!context.progress) for(auto const &actor : combat.actors) {
      std::cerr << "Final actor " << unsigned{actor.index} << " slot " << unsigned{actor.definition_slot}
        << " flags " << unsigned{actor.flags} << " callback " << actor.parameters.update_entry
        << " position " << actor.pose.position[0] << ',' << actor.pose.position[1] << ',' << actor.pose.position[2]
        << std::endl;
    }
    if(!context.progress || !context.objectives_complete || !combat.reserves.empty())
      throw std::runtime_error{"Final battle failed to finish: removals="+std::to_string(combat.completed_objectives)
        +", reserves="+std::to_string(combat.reserves.size())+", shots="+std::to_string(shots)};
    std::cout << "Final Delphi battle: " << combat.completed_objectives << " removals and " << shots
      << " controlled gun/missile shots reach the original ending request." << std::endl;
  }
  darker::resources::font_resource const mission_font{archives.load({0,29})};
  for(uint8_t const stage : std::array<uint8_t,8>{99,101,103,105,107,109,111,113}) {
    auto const &source{campaign.scenario(stage)};
    auto const index{darker::resources::select_campaign_stage(stage).record};
    auto const &record{source.records()[index]};
    auto city{darker::game::make_city_map(archives.load({0,69}),false)};
    darker::game::assign_city_variants(city,limits);
    darker::game::apply_scenario_cells(city,record);
    darker::presentation::player briefing{archives,mission_font,source,index};
    do { briefing.advance(10000); } while(briefing.continue_page());
    darker::game::player_flight pilot;
    darker::game::initialise_skimma_pad(pilot,0x16fc,0,bank.header_at(bank.special_models()[stage == 99 ? 26 : 27]).height,stage != 99);
    pilot.scenario_configuration = static_cast<uint8_t>(record.configuration & 15);
    darker::game::weapon_ammunition second_weapon;
    darker::game::refill_skimma_weapon(second_weapon,1);
    auto groups{darker::game::make_scenario_actors(record,source,bank,pilot,second_weapon,0)};
    pilot.lifecycle.flags &= 0xef;
    pilot.supply = {};
    auto &craft{std::get<darker::game::skimma_flight_state>(pilot.craft)};
    craft.damage.shield_enabled = true;
    pilot.engine_flags = 1;
    darker::game::mission_combat combat{std::move(groups[0])};
    combat.reserves = std::move(groups[1]);
    combat.free_actors = std::move(groups[2]);
    combat.spawning.sites.clear();
    combat.spawning.halon = true;
    for(uint8_t i{0}; i < 3; ++i) darker::game::refill_skimma_weapon(combat.skimma_weapons[i].ammunition,i);
    darker::game::world_objectives objectives{.list{record.objective_cell_list}};
    darker::game::mission_script script{.continuation{*record.player_program-record.shared.offset}};
    darker::game::mission_context context{.program{source.bytes(record.shared)},
      .text{source.language(index,darker::resources::scenario_language::english)},.cells{city},
      .time_multiplier{record.time_multiplier},.text_cursor{briefing.consumed_text()}};
    uint16_t available{static_cast<uint16_t>(stage <= 101 ? 3 : 7)};
    auto const &supplementary{campaign.supplementary()};
    auto const supply_index{static_cast<size_t>(record.configuration >> 4)};
    auto const &supply_record{supplementary.records()[supply_index]};
    darker::game::mission_exchange exchange{.alternate{darker::game::mission_context_slot{supplementary.bytes(supply_record.shared),
      supplementary.language(supply_index,darker::resources::scenario_language::english),supply_record.entry_offset-supply_record.shared.offset}}};
    context.activate_reserves = [&](uint8_t const opcode,uint8_t const count){
      combat.activate_reserves(static_cast<darker::game::actor_category>(opcode-9),count,pilot.pose(),static_cast<uint16_t>(context.clock));
      return objectives.complete(record) && !combat.remaining_objectives();
    };
    context.adjust_objectives = [&](uint8_t const operand){ combat.adjust_objectives(operand); return objectives.complete(record) && !combat.remaining_objectives(); };
    context.mark_aircraft_sites = [&](std::span<std::byte const> const program){ return darker::game::prepare_halon_aircraft_sites(combat.spawning,city,program); };
    context.select_weapon = [&](uint8_t const selection){
      available = std::rotl(uint16_t{0x8000},selection);
      darker::game::select_skimma_weapon(std::span{combat.skimma_weapons}.first(pilot.upgraded ? 3 : 2),
        combat.skimma_selection,combat.skimma_ring,selection,available,static_cast<uint16_t>(context.clock));
      combat.target.clear();
    };
    context.refill_weapon = [&]{ darker::game::refill_skimma_weapon(combat.skimma_weapons[combat.skimma_selection].ammunition,combat.skimma_selection); };
    context.reset_shield = [&]{ craft.damage.shield_charge = static_cast<uint16_t>((craft.damage.shield_charge & 255) | 0xbf00); };
    context.toggle_weapons = [&](uint16_t const mask){ available ^= mask; };
    context.exchange_context = [&](auto &active){ exchange.exchange(active,context,active.continuation); };
    unsigned int shots{0};
    bool returned{false};
    uint16_t previous_target{0xffff};
    for(uint32_t clock{8}; clock < 400000 && !context.progress; clock += 8) {
      bool primary{false}, secondary{false};
      auto const building{std::ranges::find_if(combat.spawning.sites,[&](uint16_t const site){ return !(city[(site >> 8)*128+(site & 127)].state & 0x20); })};
      auto const actor{std::ranges::find_if(combat.actors,[](auto const &candidate){ return !(candidate.flags & 0x20) && (candidate.attributes & 1); })};
      uint8_t weapon{combat.skimma_selection};
      uint16_t token{0xffff};
      if(building != combat.spawning.sites.end()) {
        auto const aim{darker::game::resolve_map_guidance(*building,city,bank,0x60)};
        std::array<uint16_t,3> const target{aim.position[0],aim.position[1],aim.height};
        pilot.pose().position = {target[0],static_cast<uint16_t>(target[1]+32),static_cast<uint16_t>(target[2]+2048)};
        auto const direction{darker::maths::object_target_direction(pilot.pose().position,target)};
        pilot.pose().angles = {direction.heading,direction.pitch,0};
        secondary = true;
        weapon = 1;
        token = *building;
      } else if(actor != combat.actors.end()) {
        auto const heading{actor->pose.angles.heading};
        auto const sine{darker::maths::original_sine[heading >> 6]};
        auto const cosine{darker::maths::original_sine[((heading >> 6)+256)%1024]};
        pilot.pose().position = {static_cast<uint16_t>(actor->pose.position[0]+((sine*200) >> 15)),
          static_cast<uint16_t>(actor->pose.position[1]+((cosine*200) >> 15)),actor->pose.position[2]};
        pilot.pose().angles = {heading,0,0};
        primary = actor->parameters.definition->impact_strength < 50;
        secondary = !primary;
        weapon = pilot.upgraded && (available & 4) ? 2 : 0;
        token = static_cast<uint16_t>(0xd986+actor->index*112);
      } else if(!returned && clock > 1024 && !combat.spawning.sites.empty()) {
        auto const pad{std::ranges::find(city,uint8_t{3},&darker::game::city_cell::type)};
        auto const pad_index{static_cast<size_t>(pad-city.begin())};
        pilot.pose().position = {static_cast<uint16_t>((pad_index%128)*256+128),static_cast<uint16_t>((pad_index/128)*256+255),512};
        pilot.pose().angles = {};
        pilot.engine_flags = 0;
        craft.damage.shield_enabled = false;
        craft.horizontal_velocity = 100;
        craft.vertical_velocity = 0xffff;
        if(!darker::game::begin_supply_approach(pilot,city,pilot.supply)) throw std::runtime_error{"Halon objective fixture could not enter its original supply pad"};
        for(auto &slot : combat.skimma_weapons) slot.flags &= 0xfe;
        exchange.enter_supply(script,context);
        returned = true;
      }
      if(!returned && secondary && (combat.skimma_selection != weapon || !(combat.skimma_weapons[weapon].flags & 1))) {
        darker::game::select_skimma_weapon(std::span{combat.skimma_weapons}.first(pilot.upgraded ? 3 : 2),
          combat.skimma_selection,combat.skimma_ring,static_cast<uint8_t>(weapon+1),available,static_cast<uint16_t>(clock));
      }
      if(token != previous_target) combat.target.clear();
      previous_target = token;
      // Control firing position and survivability; spawning/damage fidelity have independent native comparisons.
      craft.damage.shield_charge = 0xbf00;
      pilot.pose().speed = 496;
      if(returned) darker::game::advance_supply_motion(pilot,pilot.supply,context.transition_output,exchange.supplementary_active,0,8);
      combat.targeting_basis = darker::maths::make_view_basis({pilot.pose().angles.heading,pilot.pose().angles.pitch,0});
      combat.advance(pilot,city,bank,clock,8,static_cast<uint16_t>(clock^(clock-8)),primary && clock%128 == 0,
        source.bytes(record.shared),record.time_multiplier,nullptr,secondary && clock%128 == 0);
      shots += combat.player_fired;
      auto const &selected{combat.skimma_weapons[combat.skimma_selection]};
      darker::game::update_weapon_ring(selected.ammunition,combat.skimma_ring,static_cast<uint16_t>(clock),selected.flags,8);
      if(pilot.lifecycle.crashing) throw std::runtime_error{"Halon controlled objective check lost the pilot"};
      objectives.advance(city,record,0x60);
      context.clock = clock;
      context.objectives_complete = objectives.complete(record) && !combat.remaining_objectives();
      context.object_counter = static_cast<uint8_t>(combat.completed_objectives);
      context.counter = combat.world_damage_counter;
      context.object_flags = combat.status_flags(pilot.lifecycle.flags);
      context.messages.clear();
      darker::game::advance_mission_script(script,context);
    }
    if(available != (pilot.upgraded ? 7 : 3)) throw std::runtime_error{"Supply completion did not restore the craft weapon mask"};
    if(!context.progress || !returned) throw std::runtime_error{"Halon objective check incomplete: stage="+std::to_string(stage)
      +", buildings="+std::to_string(combat.world_damage_counter)+", removals="+std::to_string(combat.completed_objectives)
      +", remaining="+std::to_string(combat.remaining_objectives())+", shots="+std::to_string(shots)};
    std::cout << "Halon stage " << unsigned{stage} << ": " << unsigned{combat.world_damage_counter} << " sites, "
      << combat.completed_objectives << " counted aircraft, " << shots << " controlled shots and supply return complete." << std::endl;
  }

}
