#include "game/collision_cells.h"
#include <bit>
#include <utility>
#include "game/city_map.h"
#include "game/collision_rules.h"

namespace darker::game {

std::vector<collision_cell> swept_collision_cells(uint16_t const x, uint16_t const y,
  uint16_t const end_x, uint16_t const end_y) {
  /// 655E–662A visits cells in the original major/minor-axis order, retaining its fractional crossing bias
  using sweep = collision_sweep_format;
  std::vector<collision_cell> result;
  collision_cell cell{
    .column{static_cast<uint8_t>(x >> sweep::cell_fraction_bits)},
    .row{static_cast<uint8_t>(y >> sweep::cell_fraction_bits)}
  };
  auto const visit{[&]{
    if(cell.column < city_map_size.column && cell.row < city_map_size.row) result.push_back(cell);
  }};
  if((x >> sweep::cell_fraction_bits) == (end_x >> sweep::cell_fraction_bits)
    && (y >> sweep::cell_fraction_bits) == (end_y >> sweep::cell_fraction_bits)) {
    visit();
    return result;
  }
  int const delta_x{std::bit_cast<int16_t>(static_cast<uint16_t>(end_x - x))};
  int const delta_y{std::bit_cast<int16_t>(static_cast<uint16_t>(end_y - y))};
  unsigned int major{static_cast<unsigned int>(delta_x < 0 ? -delta_x : delta_x)};
  unsigned int minor{static_cast<unsigned int>(delta_y < 0 ? -delta_y : delta_y)};
  unsigned int major_fraction{(delta_x < 0 ? ~x : x) & sweep::cell_fraction_mask};
  unsigned int minor_fraction{(delta_y < 0 ? ~y : y) & sweep::cell_fraction_mask};
  auto *major_cell{&cell.column};
  auto *minor_cell{&cell.row};
  int major_step{delta_x < 0 ? -1 : 1};
  int minor_step{delta_y < 0 ? -1 : 1};
  if(minor >= major) {
    std::swap(major, minor);
    std::swap(major_fraction, minor_fraction);
    std::swap(major_cell, minor_cell);
    std::swap(major_step, minor_step);
  }
  unsigned int const slope{major == minor ? sweep::error_mask : (minor << sweep::slope_fraction_bits) / major};
  unsigned int error{(major == minor
    ? (sweep::cell_fraction_mask - major_fraction) << sweep::cell_fraction_bits
    : (sweep::cell_fraction_mask - major_fraction) * (slope >> (sweep::slope_fraction_bits - sweep::cell_fraction_bits)))
    + (minor_fraction << sweep::cell_fraction_bits) + sweep::crossing_bias};
  unsigned int const count{((major + major_fraction) >> sweep::cell_fraction_bits) + 1};
  bool crossing{error > sweep::error_mask};
  error &= sweep::error_mask;
  for(unsigned int i{0}; i < count; ++i) {
    if(i != 0) {
      *major_cell = static_cast<uint8_t>(*major_cell + major_step);
      visit();
      error += slope;
      crossing = error > sweep::error_mask;
      error &= sweep::error_mask;
    } else if(crossing) {
      visit();
    }
    if(crossing) *minor_cell = static_cast<uint8_t>(*minor_cell + minor_step);
    if(i == 0 || crossing) visit();
  }
  return result;
}

} // namespace darker::game
