#include "mission_combat_check.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <utility>
#include "game/hangar.h"
#include "game/mission_combat.h"
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
  for(size_t mission{0}; mission < 3; ++mission) {
    auto const &record{scenario.records()[mission]};
    darker::game::mission_combat combat{darker::game::make_scenario_group(record.groups[0], bank, 1, 0, record.shared.offset)};
    darker::game::player_flight player;
    auto &caero{std::get<darker::game::caero_flight_state>(player.craft)};
    caero.flying = true;
    caero.energy.reserve = 0xcfff;
    combat.primary_weapon = 1;
    darker::resources::font_resource const fonts{archives.load({.archive{0}, .slot{29}})};
    auto const text{scenario.language(mission, darker::resources::scenario_language::english)};
    darker::presentation::player briefing{archives,fonts,scenario,mission};
    do { briefing.advance(4000); } while(briefing.continue_page());
    auto const cursor{briefing.consumed_text()};
    darker::game::mission_context context{.program{scenario.bytes(record.shared)}, .text{text}, .cells{cells}, .time_multiplier{record.time_multiplier}, .text_cursor{cursor}};
    darker::game::mission_script script{.continuation{*record.player_program - record.shared.offset}};
    unsigned int shots{0};
    bool message{false};
    bool saw_burst{false}, saw_trail{false};
    for(uint16_t clock{8}; clock < 30000; clock += 8) {
      auto const target{std::ranges::find_if(combat.actors, [](auto const &actor){ return !(actor.flags & 0x20); })};
      bool const fire{target != combat.actors.end() && clock % 128 == 0};
      if(target != combat.actors.end()) {
        player.pose().position = target->pose.position;
        player.pose().position[1] += 200;
        player.pose().position[2] += 92;
        player.pose().angles = {};
        player.pose().speed = 496;
      }
      combat.advance(player, cells, bank, clock, 8, static_cast<uint16_t>(clock ^ (clock - 8)), fire);
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
        if(!context.objectives_complete) throw std::runtime_error{"Return message preceded objective completion"};
        std::string const actual{reinterpret_cast<char const *>(text.data() + event.offset), event.length};
        if(actual != std::array{"Well done- you can return to base.","Mission accomplished. Return to base.","Good job, Tolly. Return to base."}[mission]) throw std::runtime_error{"Incorrect first-mission return message"};
        message = true;
      }
      if(message && script.stopped) break;
    }
    if(!message || !script.stopped || combat.completed_objectives != (mission == 2 ? 3u : 2u) || combat.remaining_objectives() != 0 || player.lifecycle.crashing) {
      throw std::runtime_error{"First mission controlled combat did not complete: shots=" + std::to_string(shots)
        + ", removed=" + std::to_string(combat.completed_objectives) + ", reserve=" + std::to_string(caero.energy.reserve)};
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
  // Follow the actual fourth-mission flatbed, with the player and aircraft excluded from this route check.
  auto const &record{scenario.records()[3]};
  auto group{darker::game::make_scenario_group(record.groups[0],bank,1,0,record.shared.offset)};
  std::erase_if(group,[](auto const &actor){ return actor.definition_slot != 31; });
  if(group.size() != 1 || !group.front().route) throw std::runtime_error{"Fourth mission is missing its flatbed route"};
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
