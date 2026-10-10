#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include "game/object_pose.h"
#include "game/projectile_pool.h"
#include "game/time.h"
#include "maths/world_coordinates.h"

#include "game/object_catalogue.h"

namespace darker::game {

struct weapon_ammunition {
  uint8_t working{0};
  uint8_t reserve{0};
};

struct skimma_weapon_slot {
  weapon_ammunition ammunition;
  uint8_t flags{0};
};

struct skimma_fire_request {
  launch_emitter const &emitter;
  uint8_t weapon{0};
  uint8_t player_flags{0};
  bool pressed{false};
  uint16_t model{0};
  clock_tick clock{0};
  uint16_t target{0xffff};
};

projectile *fire_skimma_weapon(projectile_pool &pool, skimma_weapon_slot &slot, skimma_fire_request request);

struct weapon_ring_state {
  clock_tick reload_deadline{0};
  uint16_t spread{0};
  uint16_t target_spread{0};
};

struct skimma_armament {
  static unsigned int constexpr ordinary_slot_count{2};
  static unsigned int constexpr upgraded_slot_count{object_catalogue::skimma_weapon_count};
  static uint16_t constexpr reload_delay{1024};
  static uint16_t constexpr untracked_spread{508};

  std::array<skimma_weapon_slot, upgraded_slot_count> slots{};
  weapon_ring_state ring{
    .spread{untracked_spread},
    .target_spread{untracked_spread}
  };
  uint8_t selection{};
  uint8_t reserves{};
  int8_t recoil{};
  int16_t aim_offset{};
  uint16_t dual_launch_pitch{614};

  bool select(bool upgraded, uint8_t requested, uint16_t available, clock_tick clock);
  bool reload(uint8_t weapon, clock_tick clock);
  void update_status(clock_tick clock, int16_t target, uint16_t target_count, bool upgraded);
};

struct skimma_recoil_frame {
  int8_t next{0};
  int16_t aim_offset{0};
  int16_t shot_offset{0};
};

skimma_recoil_frame calculate_skimma_recoil(int8_t previous, game_duration frame_step);
int8_t kick_skimma_recoil(int8_t current, uint8_t random_byte);

struct weapon_ring_display {
  uint8_t radius{0};
  uint8_t remaining{0};
};

maths::world_position skimma_gun_endpoint(object_pose const &player, int16_t pitch_offset, uint16_t &random_state) noexcept;

void refill_skimma_weapon(weapon_ammunition &ammunition, uint8_t weapon);
std::optional<weapon_ring_display> calculate_weapon_ring(weapon_ammunition ammunition, weapon_ring_state ring, clock_tick clock, uint8_t enable_flags);

std::optional<weapon_ring_display> update_weapon_ring(weapon_ammunition ammunition, weapon_ring_state &ring, clock_tick clock, uint8_t enable_flags, game_duration frame_step);

} // namespace darker::game
