#pragma once

#include <array>
#include <cstdint>

namespace darker::game {

struct angular_response {
  std::uint16_t rate{0};
  std::uint16_t angle_delta{0};
  std::uint16_t frame_step{0};
};

angular_response integrate_angular_rate(std::uint16_t rate, std::uint16_t impulse, std::uint16_t frame_step) noexcept;
angular_response calculate_driven_angular_response(std::uint16_t rate, std::uint16_t gain, std::uint16_t drive, std::uint16_t frame_step) noexcept;
angular_response calculate_angular_response(std::uint16_t error, std::uint16_t rate, std::uint16_t response, std::uint16_t frame_step) noexcept;
std::uint16_t fold_bank_angle(std::uint16_t angle) noexcept;
void normalise_attitude(std::array<std::uint16_t, 3> &angles) noexcept;

} // namespace darker::game
