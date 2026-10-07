#include "game/radar_coverage.h"

namespace darker::game {

bool radar_coverage::contains(uint8_t const column, uint8_t const row) const noexcept {
  /// 59A3 classifies wrapped byte coordinates into the centre, negative and positive coverage regions
  auto const classify{[](uint8_t const value){ return value < 36 ? 1u : value < 72 ? 0u : 2u; }};
  auto const x{classify(static_cast<uint8_t>(column + 54 - centre[0]))};
  auto const y{classify(static_cast<uint8_t>(row + 54 - centre[1]))};
  return (mask & (1u << (x + y*4))) != 0;
}

radar_coverage make_radar_coverage(city_map const &cells, std::array<uint16_t,2> const player, bool const underground) {
  /// 3AFD and 5A75 use radio-grid state bytes, independently of the cell's model type
  radar_coverage result;
  for(size_t axis{0}; axis < 2; ++axis) {
    auto const coordinate{static_cast<uint8_t>(player[axis] >> 8)};
    result.centre[axis] = static_cast<uint8_t>(coordinate - static_cast<uint8_t>(coordinate + 77) % 36 + 18);
  }
  if(underground) { result.mask = 0x777; return result; }
  constexpr std::array<int,3> offsets{0,-36,36};
  for(size_t y{0}; y < 3; ++y) {
    for(size_t x{0}; x < 3; ++x) {
      auto const column{static_cast<uint8_t>(result.centre[0] + offsets[x])};
      auto const row{static_cast<uint8_t>(result.centre[1] + offsets[y])};
      if(column < 128 && row < 128 && cells[row*128 + column].state < 32) result.mask |= static_cast<uint16_t>(1u << (x + y*4));
    }
  }
  return result;
}

} // namespace darker::game
