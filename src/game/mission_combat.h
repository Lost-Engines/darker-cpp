#pragma once

#include <cstdint>
#include <vector>
#include "game/aircraft_combat.h"
#include "game/aircraft_spawning.h"
#include "game/effects.h"
#include "game/player_flight.h"
#include "game/projectile_pool.h"
#include "game/weapon_target.h"

namespace darker::game {

class mission_combat {
public:
  std::vector<scenario_actor> actors;
  std::vector<scenario_actor> reserves;
  std::vector<scenario_actor> free_actors;
  aircraft_spawning spawning;
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
  bool secondary_ready{false};
  weapon_target target;
  maths::view_basis targeting_basis{maths::make_view_basis({})};
  bool player_fired{false};
  bool player_hit{false};
  unsigned int completed_objectives{0};
  uint8_t world_damage_counter{0};

  explicit mission_combat(std::vector<scenario_actor> initial);
  void spawn_aircraft(player_flight const &player, city_map const &cells, resources::geometry_bank const &bank, uint16_t clock, uint16_t frame_step);
  void update_difficulty(uint32_t clock) noexcept;
  unsigned int remaining_objectives() const noexcept;
  void advance(player_flight &player, city_map &cells, resources::geometry_bank const &bank,
    uint32_t elapsed_ticks, uint16_t frame_step, uint16_t changes, bool trigger_pressed, std::span<std::byte const> routes = {}, uint8_t script_multiplier = 50, tunnel_network const *network = nullptr, bool secondary_pressed = false);

private:
  void release_target(uint16_t token) noexcept;
};

} // namespace darker::game
