#include "game/collision_sweep.h"
#include <algorithm>
#include <bit>
#include "maths/world_coordinates.h"

namespace darker::game {
namespace {

struct fraction {
  std::int64_t numerator{0};
  std::int64_t denominator{1};
};

bool less(fraction const left, fraction const right) noexcept {
  /// Compare the projected boundary crossings exactly, without floating-point division
  return left.numerator * right.denominator < right.numerator * left.denominator;
}

int signed_word(std::uint16_t const value) noexcept {
  /// The intersection routines compare signed coordinate words
  return std::bit_cast<std::int16_t>(value);
}

} // namespace

bool sweep_collision_box(collision_box const &box, maths::world_position const &start,
  maths::world_position &end) noexcept {
  /// Reproduce the native projected box tests and 662B's quantised impact placement for local swept segments
  fraction entry{0, 1};
  fraction exit{1, 1};
  std::uint32_t impact_fraction{0};
  for(std::size_t axis{0}; axis < 3; ++axis) {
    int const from{0};
    int const to{signed_word(static_cast<std::uint16_t>(end[axis] - start[axis]))};
    int const lower{signed_word(static_cast<std::uint16_t>(box.minimum[axis] - start[axis]))};
    int const upper{signed_word(static_cast<std::uint16_t>(box.maximum[axis] - start[axis]))};
    auto const minimum{std::min(from, to)};
    auto const maximum{std::max(from, to)};
    if(minimum >= upper || maximum < lower) return false;
    int const distance{maximum - minimum};
    if(distance != 0) {
      fraction const near{to >= from ? lower - from : from - upper, distance};
      fraction const far{to >= from ? upper - from : from - lower, distance};
      if(less(entry, near)) entry = near;
      if(less(far, exit)) exit = far;
    }
    int const boundary_distance{to >= from ? lower - from : from - upper};
    if(boundary_distance > 0) {
      impact_fraction = std::max(impact_fraction, static_cast<std::uint32_t>(boundary_distance) * 65536 / static_cast<std::uint32_t>(distance + 1));
    }
  }
  if(less(exit, entry)) return false;
  auto const remainder{static_cast<std::uint16_t>(-impact_fraction)};
  for(std::size_t axis{0}; axis < 3; ++axis) {
    int const from{0};
    int const to{signed_word(static_cast<std::uint16_t>(end[axis] - start[axis]))};
    auto const minimum{std::min(from, to)};
    auto const maximum{std::max(from, to)};
    auto const extent{static_cast<std::uint32_t>(maximum - minimum + 1)};
    auto const remaining{impact_fraction == 0 ? extent : (extent * remainder) >> 16};
    end[axis] = static_cast<std::uint16_t>(start[axis] + (to >= from ? maximum - static_cast<int>(remaining) : minimum + static_cast<int>(remaining)));
  }
  return true;
}

} // namespace darker::game
