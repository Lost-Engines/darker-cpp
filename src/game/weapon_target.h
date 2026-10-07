#pragma once

#include <array>
#include <cstdint>
#include <span>
#include "game/object_pose.h"
#include "game/scenario_actor.h"
#include "maths/view_basis.h"

namespace darker::game {

struct weapon_target {
  uint16_t token{0xffff};
  uint16_t spread{508};
  int16_t horizontal{0};
  int16_t vertical{0};
  uint8_t distance{0};

  void clear() noexcept;
};

std::array<uint16_t, 3> target_ray_end(object_pose const &player) noexcept;
uint16_t acquire_caero_target(object_pose const &player, std::span<scenario_actor const> actors,
  city_map const &cells, resources::geometry_bank const &bank, uint8_t damage_mask);

void project_caero_target(weapon_target &lock, std::array<uint16_t, 3> const &player,
  std::array<uint16_t, 3> const &target, uint16_t extent, maths::view_basis const &basis,
  uint8_t secondary_weapon, uint8_t cell_type = 0, uint8_t cell_state = 0) noexcept;

} // namespace darker::game
