#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include "game/actor_navigation.h"

namespace darker::game {

std::optional<uint8_t> advance_surface_actor(scenario_actor &actor, object_pose const &player,
  std::span<scenario_actor const> active, city_map const &cells, resources::geometry_bank const &bank,
  uint8_t damage_mask, uint16_t frame_step, std::function<void(scenario_actor&, actor_course, uint8_t)> const &fire = {},
  std::function<void(scenario_actor&)> const &drop = {}, std::array<uint8_t, 4> *threat_errors = nullptr);

} // namespace darker::game
