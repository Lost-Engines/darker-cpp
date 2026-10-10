#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include "game/actor_navigation.h"
#include "game/object_impact.h"
#include "game/projectile_pool.h"
#include "game/scenario_actor.h"
#include "maths/world_coordinates.h"

namespace darker::game {

bool sweep_aircraft(object_pose const &target, uint16_t extent, uint16_t expansion,
  maths::world_position const &start, maths::world_position &end) noexcept;
scenario_actor *sweep_actor_groups(std::span<scenario_actor> actors, resources::geometry_bank const &bank,
  maths::world_position const &start, maths::world_position const &end, uint16_t expansion,
  std::span<actor_category const> categories, std::optional<uint8_t> excluded = std::nullopt);
struct actor_impact_result {
  uint16_t effect;
  bool at_actor{false};
  bool remove{false};
};

actor_impact_result hit_actor(scenario_actor &actor, uint8_t strength, uint16_t clock, uint16_t &random_state, bool underground = false);
void advance_falling_aircraft(scenario_actor &actor, uint16_t frame_step) noexcept;

struct gun_trace {
  maths::world_position start{};
  maths::world_position end{};
  bool hit{false};
};

std::optional<uint8_t> aircraft_projectile_definition(scenario_actor const &actor, uint8_t target_flags,
  actor_course course, uint8_t distance, uint16_t clock, uint8_t difficulty, bool building_attacks = false);

projectile *drop_aircraft_bomb(projectile_pool &pool, scenario_actor &actor, bool enabled, uint16_t clock, uint16_t model_token);

std::optional<gun_trace> fire_skimma_gun(scenario_actor const &actor, object_pose const &player, uint8_t player_flags,
  uint16_t player_extent, actor_course course, uint8_t distance, uint16_t clock, uint16_t changes, uint16_t &random_state, uint8_t target_protection_mask = 0x30);

} // namespace darker::game
