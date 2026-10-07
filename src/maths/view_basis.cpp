#include "maths/view_basis.h"
#include <bit>
#include "maths/sine_table.h"

namespace darker::maths {
namespace {

std::int16_t word(int const value) noexcept {
  /// Preserve wrapping intermediate words before the next signed multiplication
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

std::int16_t multiply(std::int16_t const left, std::int16_t const right) noexcept {
  /// IMUL followed by ADD/ADC retains the signed product shifted by fifteen
  return word((left * right) >> 15);
}

struct angle_components {
  std::int16_t sine;
  std::int16_t cosine;
};

angle_components components(std::uint16_t const angle) noexcept {
  /// The camera caller rounds by fifteen before selecting a 1024-entry phase
  auto const phase{static_cast<std::uint16_t>(angle + 15) >> 6};
  return {.sine{maths::original_sine[phase]}, .cosine{maths::original_sine[(phase + 256) % 1024]}};
}

} // namespace

view_basis make_view_basis(view_angles const angles) noexcept {
  /// Translate 1D63's ordered fixed-point products and 1E40's local-axis installation
  auto const pitch{components(angles.pitch)};
  auto const roll{components(angles.roll)};
  auto const heading{components(angles.heading)};
  auto const heading_sine_roll_sine{multiply(heading.sine, roll.sine)};
  auto const heading_cosine_roll_sine{multiply(heading.cosine, roll.sine)};
  auto const heading_sine_roll_cosine{multiply(heading.sine, roll.cosine)};
  auto const heading_cosine_roll_cosine{multiply(heading.cosine, roll.cosine)};
  return {{
    {
      .horizontal{word(multiply(pitch.sine, heading_cosine_roll_sine) + heading_sine_roll_cosine)},
      .vertical{word(multiply(pitch.sine, heading_cosine_roll_cosine) - heading_sine_roll_sine)},
      .depth{multiply(heading.cosine, pitch.cosine)},
    },
    {
      .horizontal{word(heading_cosine_roll_cosine - multiply(pitch.sine, heading_sine_roll_sine))},
      .vertical{word(-multiply(pitch.sine, heading_sine_roll_cosine) - heading_cosine_roll_sine)},
      .depth{word(-multiply(heading.sine, pitch.cosine))},
    },
    {
      .horizontal{multiply(roll.sine, pitch.cosine)},
      .vertical{multiply(roll.cosine, pitch.cosine)},
      .depth{word(-pitch.sine)},
    },
  }};
}

} // namespace darker::maths
