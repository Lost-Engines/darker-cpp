#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include "game/scenario_actor.h"

namespace darker::game {

bool retire_distant_actor(scenario_actor &actor, object_pose const &player, uint16_t clock) noexcept;

void place_air_reserve(scenario_actor &actor, object_pose const &player, std::span<scenario_actor const> active);
void activate_scenario_reserves(std::vector<scenario_actor> &active, std::vector<scenario_actor> &reserves,
  actor_category category, uint8_t count, object_pose const &player, uint16_t clock);

} // namespace darker::game
