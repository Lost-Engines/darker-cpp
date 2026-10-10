#pragma once

#include <array>
#include <cstdint>
#include "game/time.h"
#include "maths/view_basis.h"

namespace darker::game {

struct angular_response {
  uint16_t rate{0};
  uint16_t angle_delta{0};
  game_duration frame_step{0};
};

angular_response integrate_angular_rate(uint16_t rate, uint16_t impulse, game_duration frame_step) noexcept;
angular_response calculate_driven_angular_response(uint16_t rate, uint16_t gain, uint16_t drive, game_duration frame_step) noexcept;
angular_response calculate_angular_response(uint16_t error, uint16_t rate, uint16_t response, game_duration frame_step) noexcept;
uint16_t fold_bank_angle(uint16_t angle) noexcept;
void normalise_attitude(maths::attitude_angles &angles) noexcept;

} // namespace darker::game
