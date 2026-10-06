#include "maths/direction.h"
#include <algorithm>
#include <bit>
#include "maths/direction_table.h"

namespace darker::maths {
namespace {

bool negative(std::uint16_t const word) {
  /// Test the native sign bit without changing the underlying magnitude
  return (word & 0x8000) != 0;
}

std::uint16_t negate(std::uint16_t const word) {
  /// NEG preserves 8000h through word wrapping
  return static_cast<std::uint16_t>(-word);
}

std::uint16_t first_quadrant(std::uint16_t const x, std::uint16_t const y) {
  /// 92A6 uses a ratio table with explicit diagonal and zero-vector results
  if(x == y) return x == 0 ? 512 : 256;
  if(x < y) return direction_table[(static_cast<std::uint32_t>(x) << 8) / y];
  return static_cast<std::uint16_t>(512 - direction_table[(static_cast<std::uint32_t>(y) << 8) / x]);
}

} // namespace

std::uint16_t direction_index(std::uint16_t x, std::uint16_t y) {
  /// 927F selects quadrants using wrapped NEG results, including the signed minimum
  if(negative(x)) {
    x = negate(x);
    y = negate(y);
    if(!negative(y)) return static_cast<std::uint16_t>(first_quadrant(x, y) + 1024);
    y = negate(y);
    return static_cast<std::uint16_t>(negate(first_quadrant(x, y)) & 2047);
  }
  if(negative(y)) return static_cast<std::uint16_t>(1024 - first_quadrant(x, negate(y)));
  return first_quadrant(x, y);
}

direction_angles direction_from_displacement(std::array<std::uint16_t, 3> const &displacement) {
  /// 925C uses a maximum-axis pitch approximation and scales altitude down by eight
  auto const x{displacement[0]};
  auto const y{displacement[1]};
  auto const z{static_cast<std::uint16_t>(std::bit_cast<std::int16_t>(displacement[2]) >> 3)};
  auto const horizontal{std::max(negative(x) ? negate(x) : x, negative(y) ? negate(y) : y)};
  return {
    .heading{static_cast<std::uint16_t>(direction_index(x, y) << 5)},
    .pitch{static_cast<std::uint16_t>(direction_index(z, horizontal) << 5)},
  };
}

direction_angles object_target_direction(std::array<std::uint16_t, 3> const &position,
  std::array<std::uint16_t, 3> const &target) {
  /// 9250/CCD7 subtract positions and reverse the direction routine's heading
  auto result{direction_from_displacement({static_cast<std::uint16_t>(target[0] - position[0]),
    static_cast<std::uint16_t>(target[1] - position[1]), static_cast<std::uint16_t>(target[2] - position[2])})};
  result.heading = static_cast<std::uint16_t>(result.heading + 0x8000);
  return result;
}

} // namespace darker::maths
