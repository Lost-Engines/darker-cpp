#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace darker::maths {

struct world_format {
  using position_component = uint16_t;
  using fraction_component = uint8_t;
  static unsigned int constexpr cell_fraction_bits{8};                        // low position byte locates the point within a map cell
  static unsigned int constexpr fraction_bits{8};                             // additional sub-position byte retained by movement
  static int constexpr units_per_cell{1 << cell_fraction_bits};
};

// components follow the native column, row, height order; arithmetic remains in the caller's units
template<typename T>
struct map_coordinates {
  T column{};
  T row{};

  // indexed access is for the original axis-wise algorithms, without pointer arithmetic between members
  constexpr T &operator[](size_t axis) noexcept {
    assert(axis < 2);
    return *std::array{&column, &row}[axis];
  }
  constexpr T const &operator[](size_t axis) const noexcept {
    assert(axis < 2);
    return *std::array{&column, &row}[axis];
  }
  static constexpr size_t size() noexcept {
    return 2;
  }
  bool operator==(map_coordinates const&) const = default;
};

template<typename T>
struct world_coordinates {
  T column{};
  T row{};
  T height{};

  // indexed access is for the original axis-wise algorithms, without pointer arithmetic between members
  constexpr T &operator[](size_t axis) noexcept {
    assert(axis < 3);
    return *std::array{&column, &row, &height}[axis];
  }
  constexpr T const &operator[](size_t axis) const noexcept {
    assert(axis < 3);
    return *std::array{&column, &row, &height}[axis];
  }
  static constexpr size_t size() noexcept {
    return 3;
  }
  bool operator==(world_coordinates const&) const = default;
};

using map_position = map_coordinates<world_format::position_component>;
using map_fractions = map_coordinates<world_format::fraction_component>;

using world_position = world_coordinates<world_format::position_component>;
using position_fractions = world_coordinates<world_format::fraction_component>;

} // namespace darker::maths
