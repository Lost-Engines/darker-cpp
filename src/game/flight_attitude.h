#pragma once

#include <cstdint>

namespace darker::game {

struct flight_turn {
  int16_t heading_delta{0};
  int16_t lift_projection{0};
};

int16_t project_flight_pitch(uint16_t pitch, uint16_t bank, uint16_t steering_delta) noexcept;
flight_turn couple_flight_turn(uint16_t bank, uint16_t pitch, uint16_t steering_delta, uint16_t frame_step) noexcept;

} // namespace darker::game
