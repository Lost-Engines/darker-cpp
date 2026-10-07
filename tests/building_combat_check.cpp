#include "building_combat_check.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include "game/hangar.h"
#include "game/mission_combat.h"
#include "game/projectile_steering.h"
#include "game/scenario_world.h"
#include "maths/direction.h"
#include "presentation/player.h"
#include "resources/archive_set.h"
#include "resources/campaign.h"

void check_building_combat(darker::resources::archive_set const &archives) {
  /// Fire Brent Ground into real marked buildings, then follow cell objectives through messages and destination docking
  darker::resources::geometry_bank const bank{archives.load({0,30})};
  darker::resources::font_resource const fonts{archives.load({0,29})};
  darker::resources::campaign_resources campaign{archives};
  std::array<uint8_t,256> limits{};
  for(size_t i{0}; i < bank.city_types().size(); ++i) limits[i+1] = bank.city_types()[i].variant_limit;
  struct building_case { uint8_t stage; size_t count; char const *message; };
  for(auto const test : std::array<building_case,4>{{
    {50,5,"Return to Hemmersan, Tolly."},{51,3,"Well done- you can return to base."},{54,7,"Make it a clean job."}, {58,5,"Return to base for a mission update."},
  }}) {
    auto const &scenario{campaign.scenario(test.stage)};
    auto const index{darker::resources::select_campaign_stage(test.stage).record};
    auto const &record{scenario.records()[index]};
    auto cells{darker::game::make_city_map(archives.load({0,68}),true)};
    darker::game::assign_city_variants(cells,limits);
    darker::game::apply_scenario_cells(cells,record);
    auto const &targets{record.cell_lists.at(1).cells};
    if(targets.size() != test.count) throw std::runtime_error{"Ground mission has an unexpected marked target count"};
    darker::game::mission_combat combat{darker::game::make_scenario_group(record.groups[0],bank,1,0,record.shared.offset)};
    combat.secondary_weapon = 6;
    combat.difficulty = test.stage*2;
    darker::game::player_flight player;
    auto &craft{std::get<darker::game::caero_flight_state>(player.craft)};
    craft.flying = true;
    craft.energy.reserve = 0xcfff;
    darker::presentation::player briefing{archives,fonts,scenario,index};
    do { briefing.advance(4000); } while(briefing.continue_page());
    if(test.stage == 50 && briefing.weapon_toggles != 32) throw std::runtime_error{"Brent Ground introduction failed to unlock key 6"};
    darker::game::world_objectives objectives{.list{record.objective_cell_list}};
    darker::game::mission_context context{.program{scenario.bytes(record.shared)},
      .text{scenario.language(index,darker::resources::scenario_language::english)},.cells{cells},
      .time_multiplier{record.time_multiplier},.text_cursor{briefing.consumed_text()}};
    darker::game::mission_script script{.continuation{*record.player_program-record.shared.offset}};
    bool message{false};
    unsigned int shots{0};
    uint16_t previous_target{0xffff};
    for(uint32_t clock{8}; clock < 200000; clock += 8) {
      auto const target{std::ranges::find_if(targets,[&](auto const cell){ return !(cells[cell.row*128+cell.column].state & 0x20); })};
      if(target != targets.end()) {
        auto const token{static_cast<uint16_t>(target->row*256+target->column)};
        auto const aim{darker::game::resolve_map_guidance(token,cells,bank,0x20)};
        std::array<uint16_t,3> const centre{aim.position[0],aim.position[1],static_cast<uint16_t>(aim.height-aim.height_extent/2)};
        constexpr std::array<int,4> columns{0,200,0,-200}, rows{200,0,-200,0};
        auto const approach{test.stage >= 54 ? (clock/2048)%4 : 0};
        player.pose().position = {static_cast<uint16_t>(centre[0]+columns[approach]),static_cast<uint16_t>(centre[1]+rows[approach]),static_cast<uint16_t>(aim.height+512)};
        auto const direction{darker::maths::object_target_direction(player.pose().position,centre)};
        player.pose().angles = {direction.heading,direction.pitch,0};
        player.pose().speed = 496;
        combat.targeting_basis = darker::maths::make_view_basis({direction.heading,direction.pitch,0});
        if(token != previous_target) combat.target.clear();
        previous_target = token;
      }
      combat.advance(player,cells,bank,clock,8,static_cast<uint16_t>(clock^(clock-8)),false,
        scenario.bytes(record.shared),record.time_multiplier,nullptr,target != targets.end() && clock%256 == 0);
      shots += combat.player_fired;
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
    if(!script.stopped || !message || !context.objectives_complete || static_cast<size_t>(destroyed) != test.count) {
      throw std::runtime_error{"Ground mission did not complete: stage="+std::to_string(test.stage)+", destroyed="+std::to_string(destroyed)
        +", shots="+std::to_string(shots)+", lock="+std::to_string(combat.target.token)+", wanted="+std::to_string(previous_target)
        +", stopped="+std::to_string(script.stopped)+", message="+std::to_string(message)};
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
      << " controlled Brent Ground launches, final message and docking verified." << std::endl;
  }
}
