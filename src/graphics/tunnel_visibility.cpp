#include "graphics/tunnel_visibility.h"
#include <array>
#include <stdexcept>

namespace darker::graphics {

void visit_tunnel_cells(std::span<game::city_cell const,game::city_map_cell_count> const cells, uint8_t const column, uint8_t const row,
  std::span<uint8_t,game::city_map_cell_count> const visibility, std::function<bool(uint16_t)> const &visit) {
  /// 2778 propagates model acceptance through the original cardinal, diagonal and eightfold outward visits
  unsigned int constexpr cardinal_reach_cells{8};
  unsigned int constexpr diagonal_reach_cells{7};
  unsigned int constexpr outer_edge_width_cells{3};
  if(column >= game::city_map_size.column || row >= game::city_map_size.row) throw std::out_of_range{"Underground camera cell exceeds its map"};
  // addresses retain the original two-byte cell layout: 128 cells per row means a 256-byte row stride
  // even addresses select cell types; odd neighbour offsets select state bytes, with /2 recovering the cell index
  uint16_t const centre{static_cast<uint16_t>(row*256 + column*2)};
  auto const occupied{[&](uint16_t const address){ return address / 2 < cells.size() && cells[address / 2].type != 0; }};
  auto const visible{[&](uint16_t const address){ return address / 2 < visibility.size() && (visibility[address / 2] & 1) != 0; }};
  auto const offset{[](uint16_t const address, int const delta){ return static_cast<uint16_t>(address + delta); }};
  auto const accept{[&](uint16_t const address){ visibility[address / 2] = static_cast<uint8_t>(visit(address / 2)); }};
  if(occupied(centre)) {
    visibility[centre / 2] = 1;
    visit(centre / 2); // The containing cell seeds propagation even if its model is culled.
  }
  for(int const step : {256,-256,-2,2}) {
    auto address{centre};
    for(unsigned int count{0}; count < cardinal_reach_cells; ++count) {
      address = offset(address,step);
      if(!occupied(address)) {
        // 29AB clears the remainder of a blocked cardinal ray, retaining the empty cell's own bit.
        for(++count; count < cardinal_reach_cells; ++count) {
          address = offset(address,step);
          if(address / 2 < visibility.size()) visibility[address / 2] = 0;
        }
        break;
      }
      accept(address);
    }
  }
  auto const examine{[&](uint16_t const address, int const first, int const second, int const third, bool const diagonal){
    if(!occupied(address)) return;
    // Reads precede clearing the destination, just as the native state-bit instructions do.
    bool const a{visible(offset(address,first))}, b{visible(offset(address,second))}, c{visible(offset(address,third))};
    visibility[address / 2] = 0;
    if(diagonal ? ((a || b) && c) : ((a && b) || c)) accept(address);
  }};
  // 258 steps one row and one column; 512 crosses two rows, while distance*4 crosses twice the column distance
  // masking with 0xff00 preserves the row; the byte cast wraps column movement without carrying into that row
  auto const diagonals{[&](unsigned int const distance){
    auto address{offset(centre,static_cast<int>(distance*258))};
    examine(address,-255,-1,-257,true);
    address = static_cast<uint16_t>((address & 0xff00) | static_cast<uint8_t>(address - distance*4));
    examine(address,-255,3,-253,true);
    address = static_cast<uint16_t>(address - distance*512);
    examine(address,257,3,259,true);
    address = static_cast<uint16_t>((address & 0xff00) | static_cast<uint8_t>(address + distance*4));
    examine(address,257,-1,255,true);
  }};
  auto const sides{[&](unsigned int const distance, unsigned int const count){
    for(unsigned int across{1}; across <= count; ++across) {
      auto address{offset(centre,static_cast<int>(across*256 + distance*2))};
      examine(address,-257,-255,-1,false);
      address = static_cast<uint16_t>((address & 0xff00) | static_cast<uint8_t>(address - distance*4));
      examine(address,-253,-255,3,false);
      address = static_cast<uint16_t>(address - across*512);
      examine(address,259,257,3,false);
      address = static_cast<uint16_t>((address & 0xff00) | static_cast<uint8_t>(address + distance*4));
      examine(address,255,257,-1,false);
      // The second half swaps the byte offsets without carrying between column and row.
      address = static_cast<uint16_t>((centre & 0xff00) | static_cast<uint8_t>(centre + across*2));
      address = offset(address,static_cast<int>(distance*256));
      examine(address,-257,-1,-255,false);
      address = static_cast<uint16_t>((address & 0xff00) | static_cast<uint8_t>(address - across*4));
      examine(address,-253,3,-255,false);
      address = static_cast<uint16_t>(address - distance*512);
      examine(address,259,3,257,false);
      address = static_cast<uint16_t>((address & 0xff00) | static_cast<uint8_t>(address + across*4));
      examine(address,255,-1,257,false);
    }
  }};
  diagonals(1);
  for(unsigned int distance{2}; distance <= diagonal_reach_cells; ++distance) {
    sides(distance,distance - 1);
    diagonals(distance);
  }
  sides(diagonal_reach_cells,diagonal_reach_cells - 1);
  sides(cardinal_reach_cells,outer_edge_width_cells);
}

} // namespace darker::graphics
