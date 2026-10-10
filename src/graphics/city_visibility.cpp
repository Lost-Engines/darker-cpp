#include "graphics/city_visibility.h"
#include <stdexcept>
#include "maths/angle.h"

namespace darker::graphics {

void collect_city_cells(std::span<game::city_cell const, game::city_map_cell_count> const cells, city_scan_rules::coordinate const column, city_scan_rules::coordinate const row,
  camera_angles const angles, unsigned int const radius, std::vector<uint16_t> &output) {
  /// 26EE traverses circular row spans, selecting the heading half unless pitch requires the full circle
  if(radius < scene_limits::minimum_scan_radius_cells || radius > scene_limits::maximum_scan_radius_cells) throw std::invalid_argument{"City scan radius exceeds its supported bounds"};
  output.clear();
  // angles wrap at 65536 units per turn; discard six bits to use the 1024-step sine-table phase
  // +15 is the native quantisation bias, not round-to-nearest (+32); its original rationale is unknown
  auto const heading_phase{maths::view_angle_phase(angles.heading)};
  auto const pitch_phase{maths::view_angle_phase(angles.pitch)};
  // 128 phase steps make a 45-degree octant; pair heading octants into four half-map scan directions
  // 0/2/4/6 are native word-table byte offsets: decreasing columns, increasing rows, increasing columns, decreasing rows
  auto const scan_direction_offset{((heading_phase >> 7) - 1) & 6};
  // fold pitch modulo half a turn: octants 1/2 select steep views towards either vertical pole
  auto const folded_pitch_octant{(pitch_phase >> 7) & 3};
  bool const scan_full_circle{folded_pitch_octant == 1 || folded_pitch_octant == 2};
  auto const span{[&](int const y, int const left, int const right){
    auto const wrapped_row{static_cast<city_scan_rules::coordinate>(y)};
    if(wrapped_row >= game::city_map_size.row) return;
    for(int x{left}; x <= right; ++x) {
      auto const wrapped_column{static_cast<city_scan_rules::coordinate>(x)};
      if(wrapped_column >= game::city_map_size.column) continue;
      auto const index{static_cast<uint16_t>(game::city_cell_index(wrapped_column, wrapped_row))};
      if(cells[index].type) output.push_back(index);
    }
  }};
  auto const rows{[&](int const width, int const offset){
    if(scan_full_circle) {
      span(row + offset, column - width, column + width);
      span(row - offset, column - width, column + width);
    } else if(scan_direction_offset == 0 || scan_direction_offset == 4) {
      int const left{scan_direction_offset == 0 ? column - width : column};
      int const right{scan_direction_offset == 0 ? column : column + width};
      span(row - offset, left, right);
      span(row + offset, left, right);
    } else {
      span(scan_direction_offset == 2 ? row + offset : row - offset, column - width, column + width);
    }
  }};
  unsigned int width{0};
  unsigned int offset{radius};
  // the circle stepper uses byte subtraction and carry/borrow, not an unbounded signed error accumulator
  city_scan_rules::error_accumulator error{static_cast<city_scan_rules::error_accumulator>(radius / 2)};
  do {
    --offset;
    error = static_cast<city_scan_rules::error_accumulator>(error - offset);
    unsigned int sum{0};
    do {
      ++width;
      sum = error + width;
      error = static_cast<city_scan_rules::error_accumulator>(sum);
    } while(sum < city_scan_rules::carry_modulus);
    rows(static_cast<int>(width), static_cast<int>(offset));
  } while(width < offset);
  while(offset > 0) {
    --offset;
    bool const borrow{error < offset};
    error = static_cast<city_scan_rules::error_accumulator>(error - offset);
    if(borrow) {
      ++width;
      error = static_cast<city_scan_rules::error_accumulator>(error + width);
    } else if(offset == 0) break;
    rows(static_cast<int>(width), static_cast<int>(offset));
  }
  // the native traversal always includes the complete centre row, even for a half-map scan
  span(row, column - static_cast<int>(width), column + static_cast<int>(width));
}

} // namespace darker::graphics
