#include "game/city_map.h"
#include <stdexcept>

namespace darker::game {

city_map make_city_map(std::span<std::byte const> const types, bool const energise_beacons) {
  /// BB59 expands type bytes; BB6D energises type-one cells only at the nine-cell Delphi lattice positions
  city_map result{};
  if(types.size() != result.size()) throw std::invalid_argument{"City map must contain exactly 128 by 128 type bytes"};
  for(std::size_t i{0}; i < result.size(); ++i) result[i].type = std::to_integer<std::uint8_t>(types[i]);
  if(energise_beacons) {
    for(std::size_t row{0}; row < 128; row += 9) {
      for(std::size_t column{0}; column < 128; column += 9) {
        auto &cell{result[row * 128 + column]};
        if(cell.type == 1) cell.state = 255;
      }
    }
  }
  return result;
}

void assign_city_variants(city_map &cells, std::span<std::uint8_t const, 256> const limits) {
  /// BBFC regenerates low state variants in map order, retaining existing high state bits
  std::array<std::uint8_t, 256> counters{};
  for(auto &cell : cells) {
    auto &counter{counters[cell.type]};
    cell.state = static_cast<std::uint8_t>(cell.state + counter);
    ++counter;
    if(counter >= limits[cell.type]) counter = 0;
  }
}

} // namespace darker::game
