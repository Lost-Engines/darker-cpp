#include "skimma_combat_check.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <utility>
#include "game/mission_combat.h"
#include "game/object_definitions.h"
#include "game/scenario_world.h"
#include "maths/sine_table.h"
#include "resources/archive_set.h"
#include "resources/campaign.h"

void check_skimma_combat(darker::resources::archive_set const &archives) {
  /// Exercise the Skimma primary gun against original Halon actors, retaining their movement and return fire
  darker::resources::geometry_bank const bank{archives.load({0,31})};
  darker::resources::campaign_resources campaign{archives};
  auto const &scenario{campaign.scenario(103)};
  auto const &record{scenario.records()[6]};
  auto cells{darker::game::make_city_map(archives.load({0,69}),false)};
  std::array<uint8_t,256> limits{};
  for(size_t i{0}; i < bank.city_types().size(); ++i) limits[i+1] = bank.city_types()[i].variant_limit;
  darker::game::assign_city_variants(cells,limits);
  darker::game::apply_scenario_cells(cells,record);
  auto initial{darker::game::make_scenario_group(record.groups[0],bank,1,1,record.shared.offset)};
  if(initial.size() != 4) throw std::runtime_error{"Skimma gun fixture lost its four original Halon craft"};
  darker::game::mission_combat combat{std::move(initial)};
  darker::game::player_flight player;
  player.craft = darker::game::skimma_flight_state{.damage{.shield_charge{0xbfff},.shield_enabled{true}}};
  player.engine_flags = 1;
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
    combat.advance(player,cells,bank,clock,8,static_cast<uint16_t>(clock^(clock-8)),target != combat.actors.end() && clock%128 == 0,
      scenario.bytes(record.shared),record.time_multiplier);
    shots += combat.player_fired;
    effects |= !combat.effects.gun_sounds.empty();
    darker::game::recharge_skimma_shield(craft.damage,8);
    if(player.lifecycle.crashing) throw std::runtime_error{"Controlled Halon gun run lost the shielded pilot"};
  }
  if(!combat.actors.empty() || !shots || !effects) throw std::runtime_error{"Skimma primary gun did not clear the controlled Halon actors"};
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
  std::cout << "Skimma primary gun: four original Halon craft cleared with " << shots << " controlled shots, shielded return fire and impact effects." << std::endl;
}
