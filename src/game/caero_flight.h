#pragma once

#include <cstdint>
#include <span>
#include "game/caero_energy.h"
#include "game/city_map.h"
#include "game/object_pose.h"
#include "game/player_damage.h"

namespace darker::game {

struct caero_flight_state {
  object_pose pose{};
  player_damage_state damage{};
  caero_energy_state energy{};
  std::uint16_t horizontal_velocity{0};
  std::uint16_t vertical_velocity{0};
  std::uint16_t pitch_assist_rate{0};
  std::uint16_t active_boost{0};
  std::uint16_t forward_bias{0};
  std::uint16_t repair_phase{0};
  std::uint16_t startup_energy{0};
  bool flying{false};
};

struct caero_flight_parameters {
  std::uint16_t angular_response{0};
  std::uint16_t drive_multiplier{0};
  std::int8_t vertical_bias{-90};
  std::uint16_t desired_height{0};
  std::uint16_t height_reference{0};
};

struct caero_flight_input {
  std::uint16_t bank_drive{0};
  std::uint16_t pitch_drive{0};
  std::uint8_t engine_flags{1};
  bool altitude_hold{false};
  bool brake{false};
  bool boost_cheat{false};
  bool unlimited_power{false};
};

bool activate_caero_boost(caero_flight_state &state) noexcept;
void advance_caero_flight(caero_flight_state &state, caero_flight_parameters parameters,
  caero_flight_input input, std::uint16_t frame_step, std::span<city_cell const, 128 * 128> cells);

} // namespace darker::game
