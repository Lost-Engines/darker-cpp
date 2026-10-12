#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include "game/scenario_actor.h"
#include "game/time.h"

namespace darker::game {

enum class air_reserve_mode { campaign, nightmare };

bool retire_distant_actor(scenario_actor &actor, object_pose const &player, clock_tick clock) noexcept;

void place_air_reserve(scenario_actor &actor, object_pose const &player, std::span<scenario_actor const> active, air_reserve_mode mode = air_reserve_mode::campaign);
void activate_scenario_reserves(std::vector<scenario_actor> &active, std::vector<scenario_actor> &reserves,
  actor_category category, uint8_t count, object_pose const &player, clock_tick clock, air_reserve_mode mode = air_reserve_mode::campaign);

} // namespace darker::game
