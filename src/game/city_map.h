#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include "maths/world_coordinates.h"

namespace darker::game {

struct city_cell {
  static uint8_t constexpr permitted_target{0x40};                             // objective eligibility; independent of the model's damage/variant bits
  uint8_t type{0};
  uint8_t state{0};                                                             // model-specific packed variants/damage flags, or beacon light level
};

inline maths::map_coordinates<int> constexpr city_map_size{
  .column{128},
  .row{128},
};
inline int constexpr city_map_cell_count{city_map_size.column * city_map_size.row};

using city_map = std::array<city_cell, city_map_cell_count>;

constexpr unsigned int city_cell_index(unsigned int column, unsigned int row) noexcept {
  /// Translate separate cell coordinates into row-major storage without imposing wrapping
  return row * city_map_size.column + column;
}

struct packed_cell_reference {
  static unsigned int constexpr row_shift{8};
  static unsigned int constexpr column_mask{0x7f};                            // native token encoding; independent of map allocation dimensions
  uint16_t value;

  constexpr unsigned int column() const noexcept {
    return value & column_mask;
  }
  constexpr unsigned int row() const noexcept {
    return value >> row_shift;
  }
  constexpr unsigned int index() const noexcept {
    return city_cell_index(column(), row());
  }
  static constexpr packed_cell_reference from_coordinates(unsigned int column, unsigned int row) noexcept {
    /// Encode the original bytes without silently masking or validating the supplied column
    return {static_cast<uint16_t>((row << row_shift) | column)};
  }
};


city_map make_city_map(std::span<std::byte const> types, bool energise_beacons);
void assign_city_variants(city_map &cells, std::span<uint8_t const, 256> limits);

} // namespace darker::game
