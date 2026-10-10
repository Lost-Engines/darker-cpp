#pragma once

#include <array>
#include <span>
#include "game/player_flight.h"
#include "game/scenario_actor.h"
#include "game/skimma_weapons.h"

namespace darker::game {

enum class scenario_setup_kind { halon_approach, anchor_escorts, raise_actor, escort_departure, final_approach, nightmare_player, copy_player_model };

scenario_setup_kind identify_scenario_setup(std::span<std::byte const> code);
void apply_player_scenario_setup(scenario_setup_kind kind, player_flight &player, int16_t model_height, weapon_ammunition &second_weapon);
void apply_actor_scenario_setup(scenario_setup_kind kind, scenario_actor &actor, uint16_t player_model, uint16_t clock);
struct scenario_actor_groups {
  std::vector<scenario_actor> active;
  std::vector<scenario_actor> reserves;
  std::vector<scenario_actor> free;
};

scenario_actor_groups make_scenario_actors(resources::scenario_record const &record,
  resources::scenario_resource const &resource, resources::geometry_bank const &bank, player_flight &player,
  weapon_ammunition &second_weapon, uint16_t clock, std::optional<tunnel_setup> tunnel = std::nullopt);

} // namespace darker::game
