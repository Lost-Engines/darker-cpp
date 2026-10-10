#include "game/angular_motion.h"
#include <algorithm>
#include <bit>

namespace darker::game {
namespace {

int16_t signed_word(int const value) noexcept {
  /// Interpret native intermediate words after truncation
  return std::bit_cast<int16_t>(static_cast<uint16_t>(value));
}

int16_t rounded_product(int16_t const value, uint16_t const step) noexcept {
  /// 83E6/8404 retain the CH contribution while packing the rounded product
  auto const product{static_cast<int32_t>(value) * signed_word(step)};
  return signed_word(((product + 128) >> 8) + (step & 0xff00));
}

} // anonymous namespace

angular_response integrate_angular_rate(uint16_t const rate, uint16_t const impulse, uint16_t const frame_step) noexcept {
  /// 83DF damps the driven angular rate and integrates its midpoint without crossing the driven sign
  auto const doubled{static_cast<uint16_t>(frame_step * 2)};
  auto const previous{signed_word(rate)};
  auto const candidate{signed_word(previous + signed_word(impulse))};
  auto next{signed_word(candidate - rounded_product(previous, doubled))};
  if((static_cast<uint16_t>(candidate ^ next) & 0x8000) != 0) next = 0;
  auto const midpoint{signed_word(previous + (signed_word(next - previous) >> 1))};
  return {
    .rate{static_cast<uint16_t>(next)},
    .angle_delta{static_cast<uint16_t>(rounded_product(midpoint, doubled))},
    .frame_step{static_cast<uint16_t>(doubled >> 1)},
  };
}

angular_response calculate_driven_angular_response(uint16_t const rate, uint16_t const gain,
  uint16_t const drive, uint16_t const frame_step) noexcept {
  /// 83D4 rounds the signed gain/drive product before the shared damping and integration
  auto const impulse{static_cast<uint16_t>((signed_word(gain) * signed_word(drive) + 128) >> 8)};
  return integrate_angular_rate(rate, impulse, frame_step);
}

angular_response calculate_angular_response(uint16_t const error, uint16_t const rate,
  uint16_t const response, uint16_t const frame_step) noexcept {
  /// 83BF clamps the complemented signed error and scales it before the gain stage
  int const sign{signed_word(error) < 0 ? -1 : 0};
  int const magnitude{static_cast<uint16_t>(error ^ sign)};
  auto const bounded{signed_word(std::min(magnitude, 0x2800) ^ sign)};
  auto const drive{static_cast<uint16_t>((static_cast<int32_t>(bounded) * signed_word(frame_step)) >> 8)};
  return calculate_driven_angular_response(rate, response, drive, frame_step);
}

uint16_t fold_bank_angle(uint16_t const angle) noexcept {
  /// 83A4 folds the roll quadrants for the turning response
  auto const quadrant{angle >> 14};
  return static_cast<uint16_t>(quadrant == 0 || quadrant == 3 ? -angle : angle + 0x8000);
}

void normalise_attitude(maths::attitude_angles &angles) noexcept {
  /// 23A0 folds inverted pitch with XOR and half-turns heading and roll
  auto const quadrant{angles.pitch >> 14};
  if(quadrant == 1 || quadrant == 2) {
    angles.heading = static_cast<uint16_t>(angles.heading + 0x8000);
    angles.pitch ^= 0x7fff;
    angles.roll = static_cast<uint16_t>(angles.roll + 0x8000);
  }
}

} // namespace darker::game
