#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include "game/actor_navigation.h"
#include "game/object_impact.h"
#include "game/scenario_actor.h"

namespace darker::game {

bool sweep_aircraft(object_pose const &target, uint16_t extent, uint16_t expansion,
  std::array<uint16_t, 3> const &start, std::array<uint16_t, 3> &end) noexcept;
impact_effect hit_aircraft(scenario_actor &actor, uint8_t strength, uint16_t clock, uint16_t &random_state);
void advance_falling_aircraft(scenario_actor &actor, uint16_t frame_step) noexcept;

struct gun_trace {
  std::array<uint16_t, 3> start{};
  std::array<uint16_t, 3> end{};
  bool hit{false};
};

std::optional<uint8_t> aircraft_projectile_definition(scenario_actor const &actor, uint8_t target_flags,
  actor_course course, uint8_t distance, uint16_t clock, uint8_t difficulty);

std::optional<gun_trace> fire_skimma_gun(scenario_actor const &actor, object_pose const &player, uint8_t player_flags,
  uint16_t player_extent, actor_course course, uint8_t distance, uint16_t clock, uint16_t changes, uint16_t &random_state);

} // namespace darker::game
