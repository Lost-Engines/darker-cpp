#pragma once

#include <array>
#include <cstdint>
#include "game/angular_motion.h"
#include "game/projectile_pool.h"

namespace darker::game {

struct map_guidance_target {
  std::array<std::uint16_t, 2> position{};
  std::uint16_t height{0};
  std::uint16_t height_extent{0};
};

void advance_mimic_projectile(projectile &record, object_pose const &player, uint16_t remaining, uint16_t frame_step);

void advance_homing_projectile(projectile &record, std::uint16_t target_heading, std::uint16_t target_pitch, std::uint16_t frame_step);

void advance_object_homing_projectile(projectile &record, object_pose const &target, std::uint16_t frame_step);

void advance_map_homing_projectile(projectile &record, map_guidance_target target, std::uint16_t frame_step);

} // namespace darker::game
