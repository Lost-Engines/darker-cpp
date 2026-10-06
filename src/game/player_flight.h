#pragma once

#include <span>
#include <variant>
#include "game/caero_flight.h"
#include "game/city_sweep.h"
#include "game/flight_controls.h"
#include "game/player_crash.h"
#include "game/skimma_flight.h"

namespace darker::game {

enum class flight_command { engine, altitude_hold, boost, speed_low, speed_high };

struct player_flight {
  std::variant<caero_flight_state, skimma_flight_state> craft{};
  flight_controls_state controls{};
  flight_steering look_drive{};
  player_crash_state lifecycle{};
  std::uint16_t desired_height{0};
  std::uint16_t forward_setting{248};
  std::uint8_t engine_flags{1};
  bool altitude_hold{false};
  bool upgraded{false};

  object_pose &pose() noexcept;
  object_pose const &pose() const noexcept;
  void command(flight_command command) noexcept;
  city_collision_result advance(flight_controls_input input, bool brake, std::uint16_t frame_step,
    std::uint16_t clock, resources::geometry_bank const &bank, std::span<city_cell, 128 * 128> cells);
};

} // namespace darker::game
