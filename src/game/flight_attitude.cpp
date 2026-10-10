#include "game/flight_attitude.h"
#include <bit>
#include "game/angular_motion.h"
#include "maths/sine_table.h"

namespace darker::game {
namespace {

int16_t word(int const value) noexcept {
  /// Retain word wrapping at each original coupling boundary
  return std::bit_cast<int16_t>(static_cast<uint16_t>(value));
}

int16_t sine(uint16_t const angle) noexcept {
  /// Use the flight path's unrounded sine index
  return maths::original_sine[angle >> 6];
}

int16_t cosine(uint16_t const angle) noexcept {
  /// Preserve the original extended sine table's cosine phase
  return maths::original_sine[((angle >> 6) + 256) % 1024];
}

int16_t high_product(int16_t const left, int16_t const right) noexcept {
  /// Return the signed high word consumed by the next coupling stage
  return static_cast<int16_t>((left * right) >> 16);
}

} // anonymous namespace

int16_t project_flight_pitch(uint16_t const pitch, uint16_t const bank, uint16_t const steering_delta) noexcept {
  /// 8077 scales pitch steering by bank cosine, with a pitch-dependent lower bound on its magnitude
  int const pitch_sine{sine(pitch)};
  auto gain{cosine(bank)};
  int const sine_magnitude{pitch_sine < 0 ? -pitch_sine : pitch_sine};
  int const cosine_magnitude{gain < 0 ? ~gain : gain};
  if(cosine_magnitude < sine_magnitude) gain = word(gain < 0 ? -sine_magnitude : sine_magnitude);
  return high_product(gain, word(steering_delta));
}

flight_turn couple_flight_turn(uint16_t const bank, uint16_t const pitch,
  uint16_t const steering_delta, uint16_t const frame_step) noexcept {
  /// 802D couples midpoint attitude and pitch steering into heading change and the shared lift projection
  auto const bank_turn{high_product(word(fold_bank_angle(bank)), cosine(pitch))};
  auto const impulse{word((bank_turn * word(frame_step)) >> 9)};
  auto const bank_sine{sine(bank)};
  auto const signed_drive{word(bank_sine < 0 ? -word(steering_delta) : word(steering_delta))};
  auto const bank_square{word((bank_sine * bank_sine) >> 15)};
  return {
    .heading_delta{word(impulse - high_product(bank_square, signed_drive))},
    .lift_projection{high_product(cosine(bank), cosine(pitch))},
  };
}

} // namespace darker::game
