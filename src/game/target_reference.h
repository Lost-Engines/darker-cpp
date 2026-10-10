#pragma once

#include <cstdint>
#include "game/city_map.h"
#include "game/native_object_layout.h"

namespace darker::game {

struct target_reference {
  static uint16_t constexpr object_bit{0x8000};
  static uint16_t constexpr none{native_object_layout::no_target};
  uint16_t value{none};

  constexpr bool is_none() const noexcept {
    return value == none;
  }
  constexpr bool is_object_encoded() const noexcept {
    /// Test the native sign bit, including the no-target sentinel
    return (value & object_bit) != 0;
  }
  constexpr bool is_ground_encoded() const noexcept {
    return !is_object_encoded();
  }
  constexpr bool permits_air_weapon() const noexcept {
    /// INC/sign-test excludes FFFF but accepts 7FFF; retain this separate native predicate
    return (static_cast<uint16_t>(value + 1) & object_bit) != 0;
  }
  constexpr packed_cell_reference cell() const noexcept {
    /// Decode a caller-classified ground reference without changing native out-of-range handling
    return {value};
  }
};

} // namespace darker::game
