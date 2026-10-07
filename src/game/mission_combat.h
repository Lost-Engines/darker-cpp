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
  std::vector<scenario_actor> reserves;
  projectile_pool projectiles;
  projectile_pool hostile_projectiles{projectile_list::hostile};
  effect_system effects;
  uint16_t random_state{0};
  uint8_t primary_weapon{0};
  uint8_t secondary_weapon{0};
  uint8_t difficulty{2};
  bool missile_camera_enabled{false};
  bool building_attacks{false};
  projectile *camera_projectile{nullptr};
  bool weapon_ready{false};
  bool player_fired{false};
  bool player_hit{false};
  unsigned int completed_objectives{0};
  uint8_t world_damage_counter{0};

  explicit mission_combat(std::vector<scenario_actor> initial);
  void update_difficulty(uint32_t clock) noexcept;
  unsigned int remaining_objectives() const noexcept;
  void advance(player_flight &player, city_map &cells, resources::geometry_bank const &bank,
    uint32_t elapsed_ticks, uint16_t frame_step, uint16_t changes, bool trigger_pressed, std::span<std::byte const> routes = {}, uint8_t script_multiplier = 50, tunnel_network const *network = nullptr);
};

} // namespace darker::game
