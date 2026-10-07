#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include "game/scenario_actor.h"

namespace darker::game {

inline constexpr std::array<uint16_t,8> delphi_aircraft_sites{0x0355,0x0b70,0x1b33,0x3754,0x3757,0x5e6a,0x6612,0x7052};

struct aircraft_spawning {
  std::array<uint16_t,8> timers{};
  std::array<int16_t,8> platforms{};
  bool enabled{true};
};

void prepare_delphi_aircraft_sites(aircraft_spawning &state, city_map &cells) noexcept;

void advance_aircraft_spawning(aircraft_spawning &state, std::vector<scenario_actor> &active,
  std::vector<scenario_actor> &free, city_map const &cells, resources::geometry_bank const &bank,
  object_pose const &player, uint16_t clock, uint16_t frame_step, uint16_t &random_state);
void advance_aircraft_departure(scenario_actor &actor, uint16_t clock, uint16_t frame_step) noexcept;

} // namespace darker::game
