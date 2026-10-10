#pragma once

#include <cstdint>
#include "maths/sine_table.h"

namespace darker::maths {

struct angle_format {
  using angle = uint16_t;
  static unsigned int constexpr full_turn{65536};
  static unsigned int constexpr half_turn{full_turn / 2};
  static unsigned int constexpr quarter_turn{full_turn / 4};
  static unsigned int constexpr sine_phase_count{1024};
  static unsigned int constexpr quarter_phase{sine_phase_count / 4};
  static unsigned int constexpr phase_shift{6};
  static unsigned int constexpr view_bias{15};                               // native renderer bias; deliberately not round-to-nearest
};

static_assert(original_sine.size() == angle_format::sine_phase_count);
static_assert((angle_format::sine_phase_count << angle_format::phase_shift) == angle_format::full_turn);

constexpr unsigned int angle_phase(angle_format::angle angle) noexcept {
  /// Flight and object orientation truncate the native angle without a bias
  return angle >> angle_format::phase_shift;
}

constexpr unsigned int view_angle_phase(angle_format::angle angle) noexcept {
  /// Camera quantisation adds its bias before wrapping the angle word
  return angle_phase(static_cast<angle_format::angle>(angle + angle_format::view_bias));
}

constexpr int16_t phase_cosine(unsigned int phase) noexcept {
  /// Cosine uses the same authored samples with a wrapped quarter-turn offset
  return original_sine[(phase + angle_format::quarter_phase) % angle_format::sine_phase_count];
}

constexpr int16_t angle_sine(angle_format::angle angle) noexcept {
  return original_sine[angle_phase(angle)];
}

constexpr int16_t angle_cosine(angle_format::angle angle) noexcept {
  return phase_cosine(angle_phase(angle));
}

} // namespace darker::maths
