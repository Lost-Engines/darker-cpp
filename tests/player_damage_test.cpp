#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include "game/player_damage.h"
#include "reference/player_damage_samples.h"

TEST_CASE("Player damage and kick match every incoming byte for all three craft", "[game][damage]") {
  /// Include the cheat patch while retaining its preceding kick and RNG consumption
  for(auto const &sample : darker::test_reference::player_damage_samples) {
    CAPTURE(sample.craft, sample.cheat, sample.amount, sample.amplitude, sample.damage, sample.charge, sample.enabled);
    darker::game::player_damage_state state{
      .rotation{.pitch{static_cast<std::uint16_t>(sample.pitch)}, .turn{static_cast<std::uint16_t>(sample.heading)}},
      .damage{static_cast<std::uint16_t>(sample.damage)},
      .shield_charge{static_cast<std::uint16_t>(sample.charge)},
      .shield_enabled{sample.enabled != 0},
    };
    auto seed{static_cast<std::uint16_t>(sample.seed)};
    darker::game::apply_player_damage(state, static_cast<std::uint8_t>(sample.amount), static_cast<std::uint8_t>(sample.amplitude),
      sample.craft != 25, sample.cheat != 0, seed);
    std::array<int, 5> const actual{state.rotation.pitch, state.rotation.turn, state.damage, state.shield_charge, seed};
    CHECK(actual == sample.result);
    CHECK(darker::game::player_damage_is_lethal(state) == (sample.result[2] >= 1024));
  }
}

TEST_CASE("Skimma fractional shield recharge matches native overflow behaviour", "[game][damage]") {
  /// Check both normal saturation and deliberately oversized timesteps
  for(auto const &sample : darker::test_reference::recharge_samples) {
    CAPTURE(sample.charge, sample.step);
    darker::game::player_damage_state state{.shield_charge{static_cast<std::uint16_t>(sample.charge)}};
    darker::game::recharge_skimma_shield(state, static_cast<std::uint16_t>(sample.step));
    CHECK(state.shield_charge == sample.result);
  }
}

TEST_CASE("Caero peripheral repair preserves the native phase and borrow rules", "[game][damage]") {
  /// Major damage does not repair through this callback
  for(auto const &sample : darker::test_reference::repair_samples) {
    CAPTURE(sample.damage, sample.phase, sample.step);
    darker::game::player_damage_state state{.damage{static_cast<std::uint16_t>(sample.damage)}};
    auto phase{static_cast<std::uint16_t>(sample.phase)};
    darker::game::repair_caero_damage(state, phase, static_cast<std::uint16_t>(sample.step));
    CHECK(state.damage == sample.result_damage);
    CHECK(phase == sample.result_phase);
  }
}
