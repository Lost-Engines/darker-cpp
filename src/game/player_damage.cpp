#include "game/player_damage.h"

namespace darker::game {

void apply_player_damage(player_damage_state &state, std::uint8_t const amount, std::uint8_t const kick_amplitude,
  bool const skimma, bool const damage_cheat, std::uint16_t &random_state) noexcept {
  /// Apply 84D0's kick before the cheat patch; Caero damage and Skimma shield depletion follow different rules
  apply_impact_rotation(state.rotation, kick_amplitude, random_state);
  auto const incoming{damage_cheat ? 0u : static_cast<unsigned int>(amount)};
  if(skimma) {
    auto const reserve{static_cast<unsigned int>(state.shield_charge >> 8)};
    bool const fatal{!state.shield_enabled || reserve <= incoming};
    if(state.shield_enabled) {
      state.shield_charge = static_cast<std::uint16_t>((state.shield_charge & 255) | (fatal ? 0 : (reserve - incoming) << 8));
    }
    state.damage = static_cast<std::uint16_t>(incoming + (fatal ? 1024 : 0));
    return;
  }
  auto const contribution{incoming & 128 ? 512 + (incoming & 127) : incoming};
  state.damage = static_cast<std::uint16_t>(state.damage + contribution);
  while((state.damage & 255) >= 68) state.damage = static_cast<std::uint16_t>(state.damage - 0xff44);
}

bool player_damage_is_lethal(player_damage_state const &state) noexcept {
  /// The 6F45 gate admits the separate crash transition at four major damage units
  return (state.damage >> 8) >= 4;
}

void recharge_skimma_shield(player_damage_state &state, std::uint16_t const frame_step) noexcept {
  /// Preserve 8108's fractional recharge and one-byte correction, including word wrap at extreme timesteps
  state.shield_charge = static_cast<std::uint16_t>(state.shield_charge + frame_step * 2);
  if((state.shield_charge >> 8) >= 192) state.shield_charge -= 256;
}

void repair_caero_damage(player_damage_state &state, std::uint16_t &repair_phase, std::uint16_t const frame_step) noexcept {
  /// 8514 repairs one peripheral unit on phase carry; the original 8523 operand is zero
  auto const phase{(repair_phase | 0xfe00u) + frame_step};
  repair_phase = static_cast<std::uint16_t>(phase);
  if(phase > 65535 && (state.damage & 255) != 0) --state.damage;
}

} // namespace darker::game
