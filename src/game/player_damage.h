#pragma once

#include <cstdint>
#include "game/object_impact.h"

namespace darker::game {

struct player_damage_state {
  impact_rotation rotation{};
  std::uint16_t damage{0};
  std::uint16_t shield_charge{0};
  bool shield_enabled{false};
};

void apply_player_damage(player_damage_state &state, std::uint8_t amount, std::uint8_t kick_amplitude,
  bool skimma, bool damage_cheat, std::uint16_t &random_state) noexcept;
bool player_damage_is_lethal(player_damage_state const &state) noexcept;
void recharge_skimma_shield(player_damage_state &state, std::uint16_t frame_step) noexcept;
void repair_caero_damage(player_damage_state &state, std::uint16_t &repair_phase,
  std::uint16_t frame_step) noexcept;

} // namespace darker::game
