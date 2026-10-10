#include "nightmare_check.h"
#include <iostream>
#include <stdexcept>
#include "game/beacon_changes.h"
#include "game/mission_combat.h"
#include "game/scenario_setup.h"
#include "game/scenario_world.h"
#include "presentation/player.h"
#include "resources/archive_set.h"

void check_nightmare(darker::resources::archive_set const &archives) {
  /// Run the original challenge's complete script with controlled objective removals and live actor/beacon updates
  darker::resources::scenario_resource const scenario{archives.load({4,15})};
  auto const &record{scenario.records()[0]};
  darker::resources::font_resource const font{archives.load({0,29})};
  darker::presentation::player briefing{archives,font,scenario,0};
  for(unsigned int i{0}; !briefing.finished() && i < 1000; ++i) briefing.advance(32);
  if(!briefing.finished() || !briefing.entry) throw std::runtime_error{"Nightmare did not finish its initial setup"};
  darker::resources::geometry_bank const bank{archives.load({0,30})};
  auto cells{darker::game::make_city_map(archives.load({0,68}),true)};
  darker::game::apply_scenario_cells(cells,record);
  darker::game::player_flight player;
  player.pose().position = {.column{11392},.row{17024},.height{0}};
  player.pose().angles.heading = 0xc400;
  darker::game::weapon_ammunition ammunition;
  auto groups{darker::game::make_scenario_actors(record,scenario,bank,player,ammunition,0)};
  if(player.pose().position.height != 2432-bank.header_at(bank.special_models()[25]).height
    || player.pose().angles.pitch != 0xf500 || (player.lifecycle.flags & 16))
    throw std::runtime_error{"Nightmare lost its original airborne launch"};
  darker::game::mission_combat combat{std::move(groups.active)};
  combat.reserves = std::move(groups.reserves);
  combat.free_actors = std::move(groups.free);
  combat.difficulty = briefing.difficulty.value_or(0);
  darker::game::beacon_changes beacons;
  darker::game::mission_script script{.continuation{*record.player_program-record.shared.offset}};
  darker::game::mission_context context{.program{scenario.bytes(record.shared)},
    .text{scenario.language(0,darker::resources::scenario_language::english)},.cells{cells},
    .time_multiplier{record.time_multiplier},.text_cursor{briefing.consumed_text()}};
  unsigned int removed{0}, waves{0}, checkpoints{0}, altitude_changes{0};
  uint8_t score{0};
  uint16_t weapons{briefing.weapon_toggles};
  context.activate_reserves = [&](uint8_t const opcode,uint8_t const count){
    ++waves;
    combat.activate_reserves(static_cast<darker::game::actor_category>(opcode-9),count,player.pose(),static_cast<uint16_t>(context.clock));
    return !combat.remaining_objectives();
  };
  context.change_beacons = [&](uint8_t const opcode,uint8_t const origin,uint8_t const count){
    beacons.command(opcode,origin,count,static_cast<uint16_t>(context.clock),scenario.bytes(record.beacon_sequence));
  };
  context.select_weapon = [&](uint8_t const selection){
    if(selection < 4) combat.primary_weapon = selection;
    else combat.secondary_weapon = selection;
  };
  context.toggle_weapons = [&](uint16_t const mask){ weapons ^= mask; };
  context.set_difficulty = [&](uint8_t const value){ combat.difficulty = value; };
  context.reset_score = [&](uint8_t const value){ ++checkpoints; score = value; combat.completed_objectives = 0; };
  context.set_altitude = [&](std::optional<uint16_t> const height){
    ++altitude_changes;
    player.scripted_altitude_hold = height.has_value();
    if(height) player.desired_height = *height;
  };
  for(uint32_t tick{8}; tick < 1000000 && !context.progress; tick += 8) {
    for(auto &actor : combat.actors) if((actor.attributes & 1) && !(actor.flags & 0x20)) {
      actor.flags |= 0x28;
      actor.expiry = static_cast<uint16_t>(tick);
      ++removed;
    }
    player.lifecycle = {};
    std::get<darker::game::caero_flight_state>(player.craft).damage = {};
    beacons.advance(cells,static_cast<uint16_t>(tick));
    combat.advance(player, cells, bank,
        {.elapsed_ticks{tick}, .frame_step{8}, .changes{static_cast<uint16_t>(tick^(tick-8))}},
        {},
        {.routes{scenario.bytes(record.shared)}, .time_multiplier{record.time_multiplier}});
    context.clock = tick;
    context.objectives_complete = !combat.remaining_objectives();
    context.object_counter = static_cast<uint8_t>(combat.completed_objectives);
    context.object_flags = combat.status_flags(player.lifecycle.flags);
    context.messages.clear();
    darker::game::advance_mission_script(script,context);
  }
  if(context.progress != 4 || !checkpoints || altitude_changes != 2 || player.scripted_altitude_hold)
    throw std::runtime_error{"Nightmare did not reach its dedicated victory outcome: offset="+std::to_string(script.continuation)
      +", progress="+std::to_string(context.progress)+", checkpoints="+std::to_string(checkpoints)};
  std::cout << "Nightmare script reaches outcome 4 at tick " << context.clock << ", " << waves << " waves, "
    << removed << " controlled removals, " << checkpoints << " score checkpoints; final score "
    << static_cast<unsigned int>(static_cast<uint8_t>(score+combat.completed_objectives)) << '.' << std::endl;
}
