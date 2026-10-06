#include "graphics/camera.h"
#include <bit>
#include "maths/sine_table.h"

namespace darker::graphics {
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

projection_term term(std::uint32_t const product) noexcept {
  /// Keep the whole word and middle byte consumed by the model projector
  return {.whole{static_cast<std::uint16_t>(product >> 16)}, .fraction{static_cast<std::uint8_t>(product >> 8)}};
}

std::uint16_t magnitude(std::int16_t const value) noexcept {
  /// The sorting estimate complements negative words, producing abs(value) minus one
  return static_cast<std::uint16_t>(value < 0 ? ~value : value);
}

} // namespace

camera_basis make_camera_basis(camera_angles const angles) noexcept {
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

camera_basis orient_model(camera_basis const &camera, camera_angles const angles) noexcept {
  /// 1E9E composes unrounded object attitude with the current camera, preserving each intermediate fixed-point product
  auto const heading{static_cast<std::uint16_t>(32768 - angles.heading) >> 6};
  auto const pitch{angles.pitch >> 6};
  auto const roll{angles.roll >> 6};
  auto const hs{maths::original_sine[heading]}, hc{maths::original_sine[(heading + 256) % 1024]};
  auto const ps{maths::original_sine[pitch]}, pc{maths::original_sine[(pitch + 256) % 1024]};
  auto const rs{maths::original_sine[roll]}, rc{maths::original_sine[(roll + 256) % 1024]};
  auto const hsrs{multiply(hs, rs)}, hcrs{multiply(hc, rs)};
  auto const hsrc{multiply(hs, rc)}, hcrc{multiply(hc, rc)};
  std::array<std::array<std::int16_t, 3>, 3> const axes{{
    {multiply(hc, pc), multiply(hs, pc), ps},
    {word(multiply(ps, hcrs) - hsrc), word(hcrc + multiply(ps, hsrs)), word(-multiply(rs, pc))},
    {word(-word(multiply(ps, hcrc) + hsrs)), word(hcrs - multiply(ps, hsrc)), multiply(rc, pc)},
  }};
  camera_basis result;
  for(std::size_t i{0}; i < axes.size(); ++i) {
    auto const project{[&](std::int16_t projection_axis::*const member){
      std::uint32_t sum{0};
      for(std::size_t j{0}; j < axes[i].size(); ++j) sum += static_cast<std::uint32_t>(axes[i][j] * (camera[j].*member));
      return word(static_cast<int>(sum >> 15));
    }};
    result[i] = {.horizontal{project(&projection_axis::horizontal)}, .vertical{project(&projection_axis::vertical)}, .depth{project(&projection_axis::depth)}};
  }
  return result;
}

model_placement place_model(camera_basis const &basis, camera_position const camera, model_origin const origin) noexcept {
  /// 2E21 transforms a cell origin while preserving word wrapping, fractional bytes and the sorting estimate
  auto const column{word(origin.column * 4 - camera.column)};
  auto const row{word(origin.row * 4 - camera.row)};
  auto const height{word(origin.height + camera.altitude)};
  auto const x{word(column * 2)};
  auto const y{word(row * 2)};
  auto const transform{[&](std::int16_t projection_axis::*const member){
    return term(static_cast<std::uint32_t>(basis[1].*member * x)
      - static_cast<std::uint32_t>(basis[0].*member * y)
      + static_cast<std::uint32_t>(basis[2].*member * height));
  }};
  return {
    .horizontal{transform(&projection_axis::horizontal)},
    .vertical{transform(&projection_axis::vertical)},
    .depth{transform(&projection_axis::depth)},
    .sorting_distance{static_cast<std::uint16_t>(magnitude(column) + magnitude(row) + magnitude(camera.altitude))},
  };
}

} // namespace darker::graphics
