#include "game/scenario_world.h"
#include <stdexcept>

namespace darker::game {

void apply_scenario_cells(city_map &cells, resources::scenario_record const &record) {
  /// C85A/C87E marks the first two scenario lists without changing the third objective list
  for(size_t list{0}; list < 2; ++list) {
    for(auto const cell : record.cell_lists[list].cells) {
      cells.at(cell.row*128 + cell.column).state |= static_cast<uint8_t>(0x20u << list);
    }
  }
}

void commit_beacon_queue(city_map &cells, std::span<std::byte const> const queue) {
  /// C569 extinguishes every queued lattice cell on mission exit, independently of gradual in-flight fading
  for(auto const packed : queue) {
    auto const value{std::to_integer<uint8_t>(packed)};
    auto const column{(value >> 4)*9};
    auto const row{(value & 15)*9};
    if(column >= 128 || row >= 128) throw std::invalid_argument{"Scenario beacon coordinate exceeds the city"};
    cells[row*128 + column].state = 0;
  }
}

void world_objectives::advance(city_map const &cells, resources::scenario_record const &record, uint8_t const damage_mask) {
  /// C82F advances one satisfied cell per frame; FE continues into the following list on a separate frame
  auto const &current{record.cell_lists.at(list)};
  if(cursor == current.cells.size()) {
    if(current.terminator == 254) { ++list; cursor = 0; }
    return;
  }
  auto const cell{current.cells.at(cursor)};
  if(cells.at(cell.row*128 + cell.column).state & damage_mask) ++cursor;
}

bool world_objectives::complete(resources::scenario_record const &record) const {
  /// C84E tests the FF terminator; reaching FE is not yet completion
  auto const &current{record.cell_lists.at(list)};
  return cursor == current.cells.size() && current.terminator == 255;
}

} // namespace darker::game
