#include "building_combat_check.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include "game/beacon_changes.h"
#include "game/city_collision.h"
#include "game/hangar.h"
#include "game/mission_combat.h"
#include "game/projectile_steering.h"
#include "game/scenario_world.h"
#include "maths/direction.h"
#include "maths/sine_table.h"
#include "presentation/player.h"
#include "resources/archive_set.h"
#include "resources/campaign.h"
#include "maths/world_coordinates.h"

void check_building_combat(darker::resources::archive_set const &archives) {
  /// Fire Brent Ground into real marked buildings, then follow cell objectives through messages and destination docking
  darker::resources::geometry_bank const bank{archives.load({0,30})};
  darker::resources::font_resource const fonts{archives.load({0,29})};
  darker::resources::campaign_resources campaign{archives};
  std::array<uint8_t,256> limits{};
  for(size_t i{0}; i < bank.city_types().size(); ++i) limits[i+1] = bank.city_types()[i].variant_limit;
  struct building_case { uint8_t stage; size_t count; char const *message; unsigned int removals{0}; };
  for(auto const test : std::array<building_case,17>{{
    {50,5,"Return to Hemmersan, Tolly."},{51,3,"Well done- you can return to base."},{54,7,"Make it a clean job."}, {58,5,"Return to base for a mission update."}, {65,8,"Tolly; we need you back at Hemmersan.",14}, {69,8,"Return to Hemmersan.",3},
    {73,9,"Mission accomplished. Return to base."}, {74,5,"Return to base for a mission update."},
    {75,19,"Return to Hemmersan."}, {76,16,"Mission complete- come back to base."}, {77,43,"Return to Hemmersan, Tolly."}, {80,4,"Return to Hemmersan immediately."},
    {81,6,"all targets are clear.",13}, {82,4,"Mission accomplished. Return to base.",17}, {87,9,"Return to Hemmersan, Tolly."},
    {94,5,"Return to base immediately.",2}, {96,9,"we advise you come back in."},
  }}) {
    auto const &scenario{campaign.scenario(test.stage)};
    auto const index{darker::resources::select_campaign_stage(test.stage).record};
    auto const &record{scenario.records()[index]};
    auto cells{darker::game::make_city_map(archives.load({0,68}),true)};
    darker::game::assign_city_variants(cells,limits);
    darker::game::apply_scenario_cells(cells,record);
    auto targets{record.cell_lists.at(1).cells};
    if(test.stage != 65 && test.stage != 77 && targets.size() != test.count) throw std::runtime_error{"Ground mission has an unexpected marked target count"};
    darker::game::mission_combat combat{darker::game::make_scenario_group(record.groups[0],bank,1,0,record.shared.offset)};
    combat.reserves = darker::game::make_scenario_group(record.groups[1],bank,static_cast<uint8_t>(1+record.groups[0].objects.size()),0,record.shared.offset);
    combat.free_actors = darker::game::make_scenario_group(record.groups[2],bank,static_cast<uint8_t>(1+record.groups[0].objects.size()+record.groups[1].objects.size()),0,record.shared.offset);
    combat.secondary_weapon = 6;
    combat.difficulty = test.stage*2;
    darker::game::player_flight player;
    auto &craft{std::get<darker::game::caero_flight_state>(player.craft)};
    craft.flying = true;
    craft.energy.reserve = 0xcfff;
    darker::presentation::player briefing{archives,fonts,scenario,index};
    do { briefing.advance(4000); } while(briefing.continue_page());
    if(test.stage == 80 && briefing.weapon_toggles != 24) throw std::runtime_error{"Diffuser introduction failed to unlock its two stages"};
    if(test.stage == 50 && briefing.weapon_toggles != 32) throw std::runtime_error{"Brent Ground introduction failed to unlock key 6"};
    darker::game::world_objectives objectives{.list{record.objective_cell_list}};
    darker::game::mission_context context{.program{scenario.bytes(record.shared)},
      .text{scenario.language(index,darker::resources::scenario_language::english)},.cells{cells},
      .time_multiplier{record.time_multiplier},.text_cursor{briefing.consumed_text()}};
    context.activate_reserves = [&](uint8_t const opcode,uint8_t const count){
      combat.activate_reserves(static_cast<darker::game::actor_category>(opcode-9),count,player.pose(),static_cast<uint16_t>(context.clock));
      return objectives.complete(record) && combat.remaining_objectives() == 0;
    };
    context.adjust_objectives = [&](uint8_t const operand){ combat.adjust_objectives(operand); return objectives.complete(record) && combat.remaining_objectives() == 0; };
    context.set_aircraft_spawning = [&](uint8_t const setting){ combat.spawning.enabled = setting != 0; };
    context.set_building_attacks = [&](uint8_t const setting){ combat.building_attacks = setting != 0; };
    darker::game::beacon_changes beacon_changes;
    context.change_beacons = [&](uint8_t const opcode,uint8_t const origin,uint8_t const count){
      beacon_changes.command(opcode,origin,count,static_cast<uint16_t>(context.clock),scenario.bytes(record.beacon_sequence));
    };
    context.replace_world_objectives = [&](std::span<std::byte const> const program){
      auto const consumed{objectives.replace(cells,program)};
      auto const &replacement{objectives.script_lists->at(0).cells};
      targets.insert(targets.end(),replacement.begin(),replacement.end());
      context.objectives_complete = objectives.complete(record) && combat.remaining_objectives() == 0;
      return consumed;
    };
    darker::game::mission_script script{.continuation{*record.player_program-record.shared.offset}};
    bool message{false};
    unsigned int shots{0};
    uint16_t previous_target{0xffff};
    for(uint32_t clock{8}; clock < 300000; clock += 8) {
      auto const target{std::ranges::find_if(targets,[&](auto const cell){ return !(cells[cell.row*128+cell.column].state & 0x20); })};
      auto const actor_target{std::ranges::find_if(combat.actors,[&](auto const &actor){
        if(actor.flags & 0x20) return false;
        // Mission 94's parked fighters are sheltered until their warehouses are opened.
        if(test.stage == 94 && target != targets.end() && actor.category == darker::game::actor_category::stationary) return false;
        if(actor.attributes & 1) return true;
        return actor.category == darker::game::actor_category::air && test.stage >= 73
          && actor.index <= record.groups[0].objects.size()+record.groups[1].objects.size();
      })};
      bool const attacking_actor{actor_target != combat.actors.end()};
      bool const warehouse{target != targets.end() && cells[target->row*128+target->column].type == 76};
      bool const ground_actor{attacking_actor && actor_target->category != darker::game::actor_category::air};
      uint8_t const weapon{static_cast<uint8_t>(attacking_actor ? (ground_actor ? 1 : 9) : warehouse ? (combat.secondary_weapon == 4 ? 4 : 5) : 6)};
      if(combat.secondary_weapon != (weapon > 3 ? weapon : 0)) combat.target.clear();
      combat.primary_weapon = weapon <= 3 ? weapon : 0;
      combat.secondary_weapon = weapon > 3 ? weapon : 0;
      if(attacking_actor) {
        auto const heading{static_cast<uint16_t>(actor_target->pose.angles.heading+(ground_actor ? 0x8000 : 0))};
        auto const sine{darker::maths::original_sine[heading >> 6]};
        auto const cosine{darker::maths::original_sine[((heading >> 6)+256)%1024]};
        player.pose().position = {static_cast<uint16_t>(actor_target->pose.position.column+((sine*200) >> 15)),
          static_cast<uint16_t>(actor_target->pose.position.row+((cosine*200) >> 15)),static_cast<uint16_t>(actor_target->pose.position.height+92+(ground_actor ? bank.header_at(actor_target->parameters.model_token).extent : 0))};
        player.pose().angles = {heading,0,0};
        player.pose().speed = 496;
        if(ground_actor && actor_target->category == darker::game::actor_category::ground) {
          auto centre{actor_target->pose.position};
          centre[2] += bank.header_at(actor_target->parameters.model_token).extent/2;
          player.pose().position = {static_cast<uint16_t>(centre[0]+32),centre[1],static_cast<uint16_t>(centre[2]+2048)};
          auto const direction{darker::maths::object_target_direction(player.pose().position,centre)};
          player.pose().angles = {direction.heading,direction.pitch,0};
        }
        combat.targeting_basis = darker::maths::make_view_basis({player.pose().angles.heading,player.pose().angles.pitch,0});
      } else if(target != targets.end()) {
        auto const token{static_cast<uint16_t>(target->row*256+target->column)};
        auto const aim{darker::game::resolve_map_guidance(token,cells,bank,0x20)};
        auto const &cell{cells[target->row*128+target->column]};
        bool const office{cell.type >= 86 && cell.type <= 89};
        darker::maths::world_position const centre{aim.position.column,aim.position.row,static_cast<uint16_t>(aim.height-aim.height_extent/2)};
        constexpr std::array<int,4> columns{0,200,0,-200}, rows{200,0,-200,0};
        auto const approach{test.stage >= 54 ? (clock/2048)%4 : 0};
        player.pose().position = {static_cast<uint16_t>(centre[0]+columns[approach]),static_cast<uint16_t>(centre[1]+rows[approach]),static_cast<uint16_t>(office ? 348 : aim.height+512)};
        if(warehouse) {
          player.pose().position = {static_cast<uint16_t>(centre[0]+32),centre[1],static_cast<uint16_t>(aim.height+2048)};
        }
        auto direction{darker::maths::object_target_direction(player.pose().position,centre)};
        if(office) {
          auto const boxes{darker::game::city_collision_boxes(bank,cell.type,cell.state,0x20,target->column,target->row)};
          auto const door{std::ranges::find_if(boxes,[](auto const &box){ return box.category == 0; })};
          if(door == boxes.end()) throw std::runtime_error{"Office target has no vulnerable entrance"};
          darker::maths::world_position point{};
          for(size_t axis{0}; axis < 3; ++axis) point[axis] = static_cast<uint16_t>((door->bounds.min[axis]+door->bounds.max[axis])/2);
          auto const along{door->bounds.max[0]-door->bounds.min[0] < door->bounds.max[1]-door->bounds.min[1] ? 1 : 0};
          auto const nearer{point[along] > centre[along] ? door->bounds.min[along] : door->bounds.max[along]};
          point[along] = static_cast<uint16_t>((point[along]+nearer)/2);
          point[2] *= 8;
          player.pose().position = point;
          for(size_t axis{0}; axis < 2; ++axis) player.pose().position[axis] = static_cast<uint16_t>(centre[axis]+static_cast<int>(2+(clock/2048)%5)*(int{point[axis]}-centre[axis]));
          player.pose().position.height += 92;
          direction = darker::maths::object_target_direction(player.pose().position,centre);
        }
        player.pose().angles = {direction.heading,office ? uint16_t{0} : direction.pitch,0};
        player.pose().speed = 496;
        combat.targeting_basis = darker::maths::make_view_basis({player.pose().angles.heading,player.pose().angles.pitch,0});
        if(token != previous_target) combat.target.clear();
        previous_target = token;
      }
      if(!attacking_actor && warehouse && weapon == 4 && static_cast<uint16_t>(clock-combat.diffuser.deadline) < 0xf400) {
        // Leave the defended roof during the gas delay; this fixture controls position rather than navigating an escape route.
        player.pose().position.column += 4096;
        player.pose().position.height += 4096;
      }
      beacon_changes.advance(cells,static_cast<uint16_t>(clock));
      bool const fire_building{target != targets.end() && clock%256 == 0
        && (!warehouse || weapon == 5 || static_cast<uint16_t>(clock-combat.diffuser.deadline) >= 0xf400)};
      combat.spawn_aircraft(player,cells,bank,static_cast<uint16_t>(clock),8);
      combat.advance(player, cells, bank,
        {.elapsed_ticks{clock}, .frame_step{8}, .changes{static_cast<uint16_t>(clock^(clock-8))}},
        {.primary_pressed{ground_actor && clock%128 == 0}, .secondary_pressed{attacking_actor ? clock%2048 == 8 : fire_building}, .secondary_held{attacking_actor && clock%2048 != 0}},
        {.routes{scenario.bytes(record.shared)}, .time_multiplier{record.time_multiplier}});
      shots += combat.player_fired;
      if(player.lifecycle.crashing) throw std::runtime_error{"Building check crash stage="+std::to_string(test.stage)+" clock="+std::to_string(clock)+" removed="+std::to_string(combat.completed_objectives)+" remaining="+std::to_string(combat.remaining_objectives())};
      darker::game::charge_caero_energy(craft.energy,13056,1,1028,false);
      objectives.advance(cells,record,0x20);
      context.clock = clock;
      context.objectives_complete = objectives.complete(record) && combat.remaining_objectives() == 0;
      context.object_counter = static_cast<uint8_t>(combat.completed_objectives);
      context.object_flags = combat.status_flags(player.lifecycle.flags);
      context.messages.clear();
      darker::game::advance_mission_script(script,context);
      for(auto const &event : context.messages) {
        std::string const text{reinterpret_cast<char const *>(event.text.data()+event.offset),event.length};
        message |= text == test.message;
      }
      if(script.stopped && message && context.objectives_complete) break;
    }
    auto const destroyed{std::ranges::count_if(targets,[&](auto const cell){ return cells[cell.row*128+cell.column].state & 0x20; })};
    if(!script.stopped || !message || !context.objectives_complete || static_cast<size_t>(destroyed) != test.count || combat.completed_objectives != test.removals) {
      throw std::runtime_error{"Ground mission did not complete: stage="+std::to_string(test.stage)+", destroyed="+std::to_string(destroyed)
        +", shots="+std::to_string(shots)+", lock="+std::to_string(combat.target.token)+", wanted="+std::to_string(previous_target)
        +", stopped="+std::to_string(script.stopped)+", message="+std::to_string(message)+", crashing="+std::to_string(player.lifecycle.crashing)};
    }
    uint16_t const destination{briefing.departure_destination ? briefing.departure_destination : uint16_t{0x7162}};
    darker::game::hangar_state hangar{.return_site{destination}};
    player.pose().position = {static_cast<uint16_t>((destination&255)*128+128),static_cast<uint16_t>((destination&0xff00)+152-700),500};
    player.pose().angles = {0x8000,0,0};
    craft.damage.rotation = {};
    if(!darker::game::begin_hangar_return(player,cells,hangar,true)) throw std::runtime_error{"Ground objectives did not permit docking"};
    for(unsigned int frame{0}; frame < 2000 && hangar.returning != darker::game::hangar_return_phase::complete; ++frame) {
      darker::game::advance_hangar_return(player,hangar,8,static_cast<uint16_t>(frame*8));
      darker::game::advance_hangar_departure(player,cells,hangar,8);
    }
    if(hangar.returning != darker::game::hangar_return_phase::complete) throw std::runtime_error{"Ground mission did not complete docking"};
    std::cout << "Mission " << unsigned{test.stage} << ": " << destroyed << " building objectives, " << shots
      << " controlled launches, " << combat.completed_objectives << " counted actor removals, final message and docking verified." << std::endl;
  }
}
