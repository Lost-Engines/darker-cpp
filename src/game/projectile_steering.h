#pragma once

#include <array>
#include <cstdint>
#include "game/angular_motion.h"
#include "game/city_map.h"
#include "game/projectile_pool.h"
#include "game/time.h"
#include "maths/world_coordinates.h"
#include "resources/geometry_bank.h"

namespace darker::game {

struct map_guidance_target {
  maths::map_position position{};
  uint16_t height{0};
  uint16_t height_extent{0};
};

map_guidance_target resolve_map_guidance(uint16_t cell, city_map const &cells, resources::geometry_bank const &bank, uint8_t damage_mask);

void advance_mimic_projectile(projectile &record, object_pose const &player, uint16_t remaining, game_duration frame_step);

void advance_chargeable_projectile(projectile &record, object_pose const &target, uint16_t remaining, game_duration frame_step);

void advance_dual_projectile(projectile &record, object_pose const &target, uint16_t separation, game_duration frame_step);

void advance_homing_projectile(projectile &record, uint16_t target_heading, uint16_t target_pitch, game_duration frame_step);

void advance_object_homing_projectile(projectile &record, object_pose const &target, game_duration frame_step);

void advance_map_homing_projectile(projectile &record, map_guidance_target target, game_duration frame_step);

} // namespace darker::game
