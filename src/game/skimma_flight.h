#pragma once

#include <cstdint>
#include "game/object_pose.h"
#include "game/player_damage.h"

namespace darker::game {

struct skimma_flight_state {
  object_pose pose{};
  player_damage_state damage{};
  uint16_t horizontal_velocity{0};
  uint16_t vertical_velocity{0};
  uint16_t pitch_assist_rate{0};
};

struct skimma_flight_parameters {
  uint16_t angular_response{0};
  int8_t vertical_bias{-106};
};

struct skimma_flight_input {
  uint16_t bank_drive{0};
  uint16_t pitch_drive{0};
  uint16_t forward_setting{248};
  bool brake{false};
};

void advance_skimma_flight(skimma_flight_state &state, skimma_flight_parameters parameters,
  skimma_flight_input input, uint16_t frame_step) noexcept;

} // namespace darker::game
