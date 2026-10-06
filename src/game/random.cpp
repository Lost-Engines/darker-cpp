#include "game/random.h"

namespace darker::game {

std::uint16_t next_random(std::uint16_t &state) noexcept {
  /// 92D2 preserves the multiply's high word, subtraction borrow and FFFF special case
  auto const incremented{static_cast<std::uint16_t>(state + 1)};
  std::uint32_t const product{static_cast<std::uint32_t>(incremented) * 75};
  auto const low{static_cast<std::uint16_t>(product)};
  auto const high{static_cast<std::uint16_t>(incremented == 0 ? 75 : product >> 16)};
  state = static_cast<std::uint16_t>(low - high + (low < high ? 1 : 0));
  return state;
}

} // namespace darker::game
