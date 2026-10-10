#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include "game/object_pose.h"
#include "game/projectile_pool.h"
#include "maths/world_coordinates.h"

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
  uint16_t clock{0};
  uint16_t target{0xffff};
};

projectile *fire_skimma_weapon(projectile_pool &pool, skimma_weapon_slot &slot, skimma_fire_request request);

struct weapon_ring_state {
  uint16_t reload_deadline{0};
  uint16_t spread{0};
  uint16_t target_spread{0};
};

struct skimma_recoil_frame {
  int8_t next{0};
  int16_t aim_offset{0};
  int16_t shot_offset{0};
};

skimma_recoil_frame calculate_skimma_recoil(int8_t previous, uint16_t frame_step);
int8_t kick_skimma_recoil(int8_t current, uint8_t random_byte);

struct weapon_ring_display {
  uint8_t radius{0};
  uint8_t remaining{0};
};

maths::world_position skimma_gun_endpoint(object_pose const &player, int16_t pitch_offset, uint16_t &random_state) noexcept;

bool select_skimma_weapon(std::span<skimma_weapon_slot> weapons, uint8_t &selected, weapon_ring_state &ring,
  uint8_t selection, uint16_t available, uint16_t clock);

void refill_skimma_weapon(weapon_ammunition &ammunition, uint8_t weapon);
bool reload_skimma_weapon(weapon_ammunition &ammunition, weapon_ring_state &ring, uint8_t weapon, uint16_t clock);
std::optional<weapon_ring_display> calculate_weapon_ring(weapon_ammunition ammunition, weapon_ring_state ring, uint16_t clock, uint8_t enable_flags);

std::optional<weapon_ring_display> update_weapon_ring(weapon_ammunition ammunition, weapon_ring_state &ring, uint16_t clock, uint8_t enable_flags, uint16_t frame_step);
uint8_t update_skimma_weapon_status(std::span<skimma_weapon_slot> weapons, weapon_ring_state &ring, uint8_t selected, uint16_t clock, int16_t target, uint16_t target_count);

} // namespace darker::game
