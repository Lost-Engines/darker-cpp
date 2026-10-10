#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>
#include "game/aircraft_combat.h"
#include "game/aircraft_spawning.h"
#include "game/caero_weapons.h"
#include "game/effects.h"
#include "game/player_flight.h"
#include "game/projectile_pool.h"
#include "game/skimma_weapons.h"
#include "game/weapon_target.h"
#include "maths/world_coordinates.h"

namespace darker::game {

struct combat_timing {
  uint32_t elapsed_ticks{};
  uint16_t frame_step{};
  uint16_t changes{};
};

struct combat_input {
  bool primary_pressed{};
  bool secondary_pressed{};
  bool secondary_held{};
  bool primary_released{};
};

struct combat_scenario {
  std::span<std::byte const> routes{};
  uint8_t time_multiplier{50};
  tunnel_network const *network{nullptr};
};

struct skimma_armament {
  std::array<skimma_weapon_slot,3> slots{};
  weapon_ring_state ring{.spread{508},.target_spread{508}};
  uint8_t selection{};
  uint8_t reserves{};
  int8_t recoil{};
  int16_t aim_offset{};
  uint16_t dual_launch_pitch{614};
};

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
  uint16_t script_owner{0};
  uint16_t weapon_charge{0};
  std::array<uint8_t,4> threat_errors{64,64,64,64};
  skimma_armament skimma;
  diffuser_state diffuser;
  uint8_t primary_weapon{0};
  uint8_t secondary_weapon{0};
  uint8_t difficulty{2};
  bool missile_camera_enabled{false};
  bool building_attacks{false};
  projectile *camera_projectile{nullptr};
  std::optional<uint8_t> camera_actor;
  bool weapon_ready{false};
  bool secondary_ready{false};
  weapon_target target;
  maths::view_basis targeting_basis{maths::make_view_basis({})};
  bool player_fired{false};
  bool player_hit{false};
  city_collision_result player_contact{};
  unsigned int completed_objectives{0};
  uint8_t world_damage_counter{0};

  explicit mission_combat(std::vector<scenario_actor> initial);
  void spawn_aircraft(player_flight const &player, city_map const &cells, resources::geometry_bank const &bank, uint16_t clock, uint16_t frame_step);
  void collide_player(player_flight &player, maths::world_position const &start,
    city_map &cells, resources::geometry_bank const &bank, uint16_t clock);
  void collide_aircraft(city_map const &cells, resources::geometry_bank const &bank, uint8_t damage_mask, uint16_t clock, bool underground = false);
  void activate_reserves(actor_category category, uint8_t count, object_pose const &player, uint16_t clock);
  void adjust_objectives(uint8_t operand) noexcept;
  void update_difficulty(uint32_t clock) noexcept;
  unsigned int remaining_objectives() const noexcept;
  std::span<uint8_t const> status_flags(uint8_t player_flags) noexcept;
  void advance(player_flight &player, city_map &cells, resources::geometry_bank const &bank,
    combat_timing timing, combat_input input = {}, combat_scenario scenario = {},
    std::optional<maths::world_position> player_start = std::nullopt);

private:
  std::array<uint8_t,256> retained_flags{};
  uint8_t outstanding_objectives{0};
  void release_target(uint16_t token) noexcept;
  void fire_skimma_primary(player_flight const &player, city_map const &cells, resources::geometry_bank const &bank, uint16_t clock, uint16_t frame_step, bool pressed);
  void detonate_dual_launch(projectile &shot, uint16_t clock, bool underground);
};

} // namespace darker::game
