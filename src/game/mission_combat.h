#pragma once

#include <cstdint>
#include <vector>
#include "game/aircraft_combat.h"
#include "game/effects.h"
#include "game/player_flight.h"
#include "game/projectile_pool.h"

namespace darker::game {

class mission_combat {
public:
  std::vector<scenario_actor> actors;
  projectile_pool projectiles;
  projectile_pool hostile_projectiles{projectile_list::hostile};
  effect_system effects;
  uint16_t random_state{0};
  uint8_t primary_weapon{0};
  uint8_t difficulty{2};
  bool weapon_ready{false};
  bool player_fired{false};
  bool player_hit{false};
  unsigned int completed_objectives{0};

  explicit mission_combat(std::vector<scenario_actor> initial);
  void update_difficulty(uint32_t clock) noexcept;
  unsigned int remaining_objectives() const noexcept;
  void advance(player_flight &player, city_map &cells, resources::geometry_bank const &bank,
    uint16_t clock, uint16_t frame_step, uint16_t changes, bool trigger_pressed, std::span<std::byte const> routes = {});
};

} // namespace darker::game
