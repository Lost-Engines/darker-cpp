#pragma once

#include <optional>
#include <span>
#include "game/city_map.h"
#include "game/object_pose.h"
#include "game/scenario_actor.h"
#include "maths/world_coordinates.h"
#include "resources/geometry_bank.h"

namespace darker::game {

struct camera_target {
  std::optional<actor_index> actor{};
  object_pose anchor;
};

maths::world_position camera_ray_end(object_pose const &camera) noexcept;
bool camera_target_in_range(object_pose const &player, object_pose const &target) noexcept;
std::optional<camera_target> pick_camera_target(object_pose const &camera, object_pose const &player,
  uint16_t player_extent, std::optional<actor_index> excluded, std::span<scenario_actor> actors,
  city_map const &cells, resources::geometry_bank const &bank, uint8_t damage_mask);

} // namespace darker::game
