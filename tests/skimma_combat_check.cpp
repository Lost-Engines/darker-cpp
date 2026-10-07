#include "skimma_combat_check.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <utility>
#include "game/mission_combat.h"
#include "game/mission_exchange.h"
#include <bit>
#include "game/object_definitions.h"
#include "game/scenario_world.h"
#include "maths/sine_table.h"
#include "resources/archive_set.h"
#include "resources/campaign.h"
#include "reference/halon_spawning_samples.h"

void check_skimma_combat(darker::resources::archive_set const &archives) {
  /// Exercise the Skimma primary gun against original Halon actors, retaining their movement and return fire
  darker::resources::geometry_bank const bank{archives.load({0,31})};
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
      actor.pose.angles[0],actor.pose.angles[1],actor.pose.angles[2],actor.pose.speed,actor.selected_target,actor.target_token,
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
        auto const heading{target->pose.angles[0]};
        auto const sine{darker::maths::original_sine[heading >> 6]};
        auto const cosine{darker::maths::original_sine[((heading >> 6)+256)%1024]};
        player.pose().position = {static_cast<uint16_t>(target->pose.position[0]+((sine*200) >> 15)),
          static_cast<uint16_t>(target->pose.position[1]+((cosine*200) >> 15)),target->pose.position[2]};
        player.pose().angles = {heading,0,0};
        player.pose().speed = 496;
      }
      combat.targeting_basis = darker::maths::make_view_basis({player.pose().angles[0],player.pose().angles[1],player.pose().angles[2]});
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

}
