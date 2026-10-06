#include "game/collision_cells.h"
#include <bit>
#include <utility>

namespace darker::game {

std::vector<collision_cell> swept_collision_cells(std::uint16_t const x, std::uint16_t const y,
  std::uint16_t const end_x, std::uint16_t const end_y) {
  /// 655E–662A visits cells in the original major/minor-axis order, retaining its fractional crossing bias
  std::vector<collision_cell> result;
  collision_cell cell{.column{static_cast<std::uint8_t>(x >> 8)}, .row{static_cast<std::uint8_t>(y >> 8)}};
  auto const visit{[&]{
    if(cell.column < 128 && cell.row < 128) result.push_back(cell);
  }};
  if((x >> 8) == (end_x >> 8) && (y >> 8) == (end_y >> 8)) {
    visit();
    return result;
  }
  int const delta_x{std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(end_x - x))};
  int const delta_y{std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(end_y - y))};
  unsigned int major{static_cast<unsigned int>(delta_x < 0 ? -delta_x : delta_x)};
  unsigned int minor{static_cast<unsigned int>(delta_y < 0 ? -delta_y : delta_y)};
  unsigned int major_fraction{(delta_x < 0 ? ~x : x) & 255u};
  unsigned int minor_fraction{(delta_y < 0 ? ~y : y) & 255u};
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
  unsigned int const slope{major == minor ? 65535 : (minor << 16) / major};
  unsigned int error{(major == minor ? (255 - major_fraction) << 8 : (255 - major_fraction) * (slope >> 8)) + (minor_fraction << 8) + 128};
  unsigned int const count{((major + major_fraction) >> 8) + 1};
  bool crossing{error > 65535};
  error &= 65535;
  for(unsigned int i{0}; i < count; ++i) {
    if(i != 0) {
      *major_cell = static_cast<std::uint8_t>(*major_cell + major_step);
      visit();
      error += slope;
      crossing = error > 65535;
      error &= 65535;
    } else if(crossing) {
      visit();
    }
    if(crossing) *minor_cell = static_cast<std::uint8_t>(*minor_cell + minor_step);
    if(i == 0 || crossing) visit();
  }
  return result;
}

} // namespace darker::game
