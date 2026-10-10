#include "graphics/camera.h"
#include <bit>
#include "vectorstorm/matrix/matrix3.h"
#include "maths/sine_table.h"

namespace darker::graphics {
namespace {

int16_t word(int const value) noexcept {
  /// Preserve wrapping intermediate words before the next signed multiplication
  return std::bit_cast<int16_t>(static_cast<uint16_t>(value));
}

int16_t multiply(int16_t const left, int16_t const right) noexcept {
  /// IMUL followed by ADD/ADC retains the signed product shifted by fifteen
  return word((left * right) >> 15);
}

projection_term term(uint32_t const product) noexcept {
  /// Keep the whole word and middle byte consumed by the model projector
  return {
    .whole{static_cast<uint16_t>(product >> 16)},
    .fraction{static_cast<uint8_t>(product >> 8)}
  };
}

uint16_t magnitude(int16_t const value) noexcept {
  /// The sorting estimate complements negative words, producing abs(value) minus one
  return static_cast<uint16_t>(value < 0 ? ~value : value);
}

} // anonymous namespace

camera_basis make_camera_basis(camera_angles const angles) noexcept {
  /// Share the native camera transform with weapon targeting
  return maths::make_view_basis(angles);
}

camera_basis orient_model(camera_basis const &camera, camera_angles const angles) noexcept {
  /// 1E9E composes unrounded object attitude with the current camera, preserving each intermediate fixed-point product
  auto const heading{static_cast<uint16_t>(32768 - angles.heading) >> 6};
  auto const pitch{angles.pitch >> 6};
  auto const roll{angles.roll >> 6};
  auto const hs{maths::original_sine[heading]}, hc{maths::original_sine[(heading + 256) % 1024]};
  auto const ps{maths::original_sine[pitch]}, pc{maths::original_sine[(pitch + 256) % 1024]};
  auto const rs{maths::original_sine[roll]}, rc{maths::original_sine[(roll + 256) % 1024]};
  auto const hsrs{multiply(hs, rs)}, hcrs{multiply(hc, rs)};
  auto const hsrc{multiply(hs, rc)}, hcrc{multiply(hc, rc)};
  mat3<int16_t> const axes{
    multiply(hc, pc), multiply(hs, pc), ps,
    word(multiply(ps, hcrs) - hsrc), word(hcrc + multiply(ps, hsrs)), word(-multiply(rs, pc)),
    word(-word(multiply(ps, hcrc) + hsrs)), word(hcrs - multiply(ps, hsrc)), multiply(rc, pc),
  };
  camera_basis result;
  for(unsigned int i{0}; i < 3; ++i) {
    auto const project{[&](int16_t projection_axis::*const member){
      uint32_t sum{0};
      for(unsigned int j{0}; j < 3; ++j) sum += static_cast<uint32_t>(axes[i, j] * (camera[j].*member));
      return word(static_cast<int>(sum >> 15));
    }};
    result[i] = {
      .horizontal{project(&projection_axis::horizontal)},
      .vertical{project(&projection_axis::vertical)},
      .depth{project(&projection_axis::depth)}
    };
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
  auto const transform{[&](int16_t projection_axis::*const member){
    return term(static_cast<uint32_t>(basis[1].*member * x)
      - static_cast<uint32_t>(basis[0].*member * y)
      + static_cast<uint32_t>(basis[2].*member * height));
  }};
  return {
    .horizontal{transform(&projection_axis::horizontal)},
    .vertical{transform(&projection_axis::vertical)},
    .depth{transform(&projection_axis::depth)},
    .sorting_distance{static_cast<uint16_t>(magnitude(column) + magnitude(row) + magnitude(camera.altitude))},
  };
}

} // namespace darker::graphics
