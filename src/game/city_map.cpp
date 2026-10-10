#include "game/city_map.h"
#include <stdexcept>
#include "game/beacon_light.h"

namespace darker::game {

city_map make_city_map(std::span<std::byte const> const types, bool const energise_beacons) {
  /// BB59 expands type bytes; BB6D energises type-one cells only at the nine-cell Delphi lattice positions
  uint8_t constexpr beacon_model_type{1};
  uint8_t constexpr fully_lit_beacon{255};
  city_map result{};
  if(types.size() != result.size()) throw std::invalid_argument{"City map must contain exactly 128 by 128 type bytes"};
  for(unsigned int i{0}; i < result.size(); ++i) result[i].type = std::to_integer<uint8_t>(types[i]);
  if(energise_beacons) {
    for(unsigned int row{0}; row < city_map_size.row; row += beacon_spacing_cells) {
      for(unsigned int column{0}; column < city_map_size.column; column += beacon_spacing_cells) {
        auto &cell{result[city_cell_index(column, row)]};
        if(cell.type == beacon_model_type) cell.state = fully_lit_beacon;
      }
    }
  }
  return result;
}

void assign_city_variants(city_map &cells, std::span<uint8_t const, 256> const limits) {
  /// BBFC regenerates low state variants in map order, retaining existing high state bits
  std::array<uint8_t, 256> counters{};
  for(auto &cell : cells) {
    auto &counter{counters[cell.type]};
    cell.state = static_cast<uint8_t>(cell.state + counter);
    ++counter;
    if(counter >= limits[cell.type]) counter = 0;
  }
}

} // namespace darker::game
