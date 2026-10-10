#include "game/radar_coverage.h"
#include "maths/world_coordinates.h"

namespace darker::game {

bool radar_coverage::contains(uint8_t const column, uint8_t const row) const noexcept {
  /// 59A3 classifies wrapped byte coordinates into the centre, negative and positive coverage regions
  auto const classify{[](uint8_t const value){ return value < region_width_cells ? 1u : value < 2 * region_width_cells ? 0u : 2u; }};
  auto const x{classify(static_cast<uint8_t>(column + classification_bias_cells - centre[0]))};
  auto const y{classify(static_cast<uint8_t>(row + classification_bias_cells - centre[1]))};
  return (mask & (1u << (x + y*mask_row_bits))) != 0;
}

radar_coverage make_radar_coverage(city_map const &cells, maths::map_position const player, bool const underground) {
  /// 3AFD and 5A75 use radio-grid state bytes, independently of the cell's model type
  using coverage = radar_coverage;
  uint8_t constexpr disabled_tower_state{32};
  radar_coverage result;
  for(unsigned int axis{0}; axis < 2; ++axis) {
    auto const coordinate{static_cast<uint8_t>(player[axis] >> 8)};
    result.centre[axis] = static_cast<uint8_t>(coordinate - static_cast<uint8_t>(coordinate + coverage::grid_alignment_bias_cells) % coverage::region_width_cells + coverage::region_width_cells / 2);
  }
  if(underground) { result.mask = coverage::full_coverage_mask; return result; }
  constexpr std::array<int,3> offsets{0,-coverage::region_width_cells,coverage::region_width_cells};
  for(unsigned int y{0}; y < 3; ++y) {
    for(unsigned int x{0}; x < 3; ++x) {
      auto const column{static_cast<uint8_t>(result.centre[0] + offsets[x])};
      auto const row{static_cast<uint8_t>(result.centre[1] + offsets[y])};
      if(column < city_map_size.column && row < city_map_size.row && cells[row * city_map_size.column + column].state < disabled_tower_state) result.mask |= static_cast<uint16_t>(1u << (x + y*coverage::mask_row_bits));
    }
  }
  return result;
}

} // namespace darker::game
