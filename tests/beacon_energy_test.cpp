#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include "game/beacon_light.h"
#include "game/caero_energy.h"
#include "reference/beacon_energy_samples.h"

TEST_CASE("Beacon power matches native lattice lookup and distance attenuation", "[game][energy]") {
  /// Supply only the cell selected by the native lookup, including invalid lattice coordinates and non-beacon types
  for(auto const &sample : darker::test_reference::beacon_samples) {
    CAPTURE(sample.x, sample.y, sample.z, sample.kind, sample.strength, sample.cell_x, sample.cell_y);
    std::array<darker::game::city_cell, 128 * 128> cells{};
    if(sample.cell_x < 128 && sample.cell_y < 128) {
      cells[sample.cell_y * 128 + sample.cell_x] = {
        .type{static_cast<std::uint8_t>(sample.kind)}, .state{static_cast<std::uint8_t>(sample.strength)},
      };
    }
    auto const light{darker::game::beacon_light(cells,
      {static_cast<std::uint16_t>(sample.x), static_cast<std::uint16_t>(sample.y), static_cast<std::uint16_t>(sample.z)},
      {static_cast<std::uint8_t>(sample.fx), static_cast<std::uint8_t>(sample.fy)})};
    CHECK(light == sample.result);
  }
}

TEST_CASE("Caero energy accounting matches native buffers, caps and boost cheat", "[game][energy]") {
  /// Exercise threshold inputs, engine flags, full reserves and word-overflow boundaries
  for(auto const &sample : darker::test_reference::energy_samples) {
    CAPTURE(sample.source, sample.flags, sample.cheat, sample.step, sample.buffer, sample.reserve, sample.boost);
    darker::game::caero_energy_state state{
      .buffer{static_cast<std::uint16_t>(sample.buffer)},
      .reserve{static_cast<std::uint16_t>(sample.reserve)},
      .boost{static_cast<std::uint16_t>(sample.boost)},
    };
    darker::game::charge_caero_energy(state, static_cast<std::uint16_t>(sample.source), static_cast<std::uint8_t>(sample.flags),
      static_cast<std::uint16_t>(sample.step), sample.cheat != 0);
    std::array<int, 5> const actual{state.buffer, state.reserve, state.boost, state.incoming_display, state.reserve_display};
    CHECK(actual == sample.result);
  }
}
