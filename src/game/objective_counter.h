#pragma once

#include <cstdint>

namespace darker::game {

constexpr uint8_t adjust_objective_counter(uint8_t const count, uint8_t const operand) noexcept {
  /// C16E adds a wrapping byte operand, then clamps a negative signed result to zero
  auto const result{static_cast<uint8_t>(count + operand)};
  return result & 0x80 ? 0 : result;
}

} // namespace darker::game
