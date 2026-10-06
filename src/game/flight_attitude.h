#pragma once

#include <cstdint>

namespace darker::game {

struct flight_turn {
  std::int16_t heading_delta{0};
  std::int16_t lift_projection{0};
};

std::int16_t project_flight_pitch(std::uint16_t pitch, std::uint16_t bank, std::uint16_t steering_delta) noexcept;
flight_turn couple_flight_turn(std::uint16_t bank, std::uint16_t pitch, std::uint16_t steering_delta, std::uint16_t frame_step) noexcept;

} // namespace darker::game
