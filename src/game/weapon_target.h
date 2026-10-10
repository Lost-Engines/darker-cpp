#pragma once

#include <array>
#include <cstdint>
#include <span>
#include "game/object_pose.h"
#include "game/scenario_actor.h"
#include "maths/view_basis.h"
#include "maths/world_coordinates.h"

namespace darker::game {

struct weapon_target {
  static uint16_t constexpr no_target{0xffff};
  static uint16_t constexpr aircraft_token_bit{0x8000};
  static uint16_t constexpr untracked_spread{508};

  uint16_t token{no_target};
  uint16_t spread{untracked_spread};
  int16_t horizontal{0};
  int16_t vertical{0};
  uint8_t distance{0};

  void clear() noexcept;

  void project_caero(maths::world_position const &player,
    maths::world_position const &target, uint16_t extent, maths::view_basis const &basis,
    uint8_t secondary_weapon, uint8_t cell_type = 0, uint8_t cell_state = 0) noexcept;

  void project_skimma(maths::world_position const &player,
    maths::world_position const &target, uint16_t extent, maths::view_basis const &basis,
    uint8_t weapon, bool enabled, bool reloading, bool destructible) noexcept;

private:
  void project(maths::world_position const &player, maths::world_position const &target,
    uint16_t extent, maths::view_basis const &basis, uint8_t secondary_weapon,
    uint8_t cell_type, uint8_t cell_state, bool skimma) noexcept;
};

maths::world_position target_ray_end(object_pose const &player) noexcept;
uint16_t acquire_caero_target(object_pose const &player, std::span<scenario_actor const> actors,
  city_map const &cells, resources::geometry_bank const &bank, uint8_t damage_mask);

} // namespace darker::game
