#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include "game/object_pose.h"

namespace darker::game {

struct weapon_ammunition {
  std::uint8_t working{0};
  std::uint8_t reserve{0};
};

struct skimma_weapon_slot {
  weapon_ammunition ammunition;
  std::uint8_t flags{0};
};

struct weapon_ring_state {
  std::uint16_t reload_deadline{0};
  std::uint16_t spread{0};
  std::uint16_t target_spread{0};
};

struct skimma_recoil_frame {
  std::int8_t next{0};
  std::int16_t aim_offset{0};
  std::int16_t shot_offset{0};
};

skimma_recoil_frame calculate_skimma_recoil(std::int8_t previous, std::uint16_t frame_step);
std::int8_t kick_skimma_recoil(std::int8_t current, std::uint8_t random_byte);

struct weapon_ring_display {
  std::uint8_t radius{0};
  std::uint8_t remaining{0};
};

std::array<uint16_t,3> skimma_gun_endpoint(object_pose const &player, int16_t pitch_offset, uint16_t &random_state) noexcept;

void refill_skimma_weapon(weapon_ammunition &ammunition, std::uint8_t weapon);
bool reload_skimma_weapon(weapon_ammunition &ammunition, weapon_ring_state &ring, std::uint8_t weapon, std::uint16_t clock);
std::optional<weapon_ring_display> calculate_weapon_ring(weapon_ammunition ammunition, weapon_ring_state ring, std::uint16_t clock, std::uint8_t enable_flags);

std::optional<weapon_ring_display> update_weapon_ring(weapon_ammunition ammunition, weapon_ring_state &ring, std::uint16_t clock, std::uint8_t enable_flags, std::uint16_t frame_step);
std::uint8_t update_skimma_weapon_status(std::span<skimma_weapon_slot> weapons, weapon_ring_state &ring, std::uint8_t selected, std::uint16_t clock, std::int16_t target, std::uint16_t target_count);

} // namespace darker::game
