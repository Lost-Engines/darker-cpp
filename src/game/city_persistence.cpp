#include "game/city_persistence.h"
#include <algorithm>
#include <stdexcept>

namespace darker::game {
namespace {

bool eligible(city_cell const &cell, std::span<resources::city_type const> const types) {
  /// Descriptor byte +4 excludes decorative types from the original packed stream
  if(cell.type == 0) return false;
  if(cell.type > types.size()) throw std::invalid_argument{"City cell has no type descriptor"};
  return types[cell.type - 1].collision_marker != resources::city_type::background_marker;
}

void validate_size(city_map const &cells, std::span<resources::city_type const> const types, size_t const bytes) {
  /// Validate the entire stream before modifying either saved or runtime state
  auto const count{std::ranges::count_if(cells,[&](auto const &cell){ return eligible(cell,types); })};
  if((static_cast<size_t>(count) + 3) / 4 != bytes) throw std::invalid_argument{"Packed city state has the wrong size"};
}

} // namespace

void pack_city_state(city_map const &cells, std::span<resources::city_type const> const types, std::span<std::byte> const packed) {
  /// BB90 writes state bits 20h/40h in row order, most significant pair first
  validate_size(cells,types,packed.size());
  std::ranges::fill(packed,std::byte{0});
  size_t index{0};
  for(auto const &cell : cells) {
    if(!eligible(cell,types)) continue;
    packed[index / 4] |= static_cast<std::byte>(((cell.state >> 5) & 3) << (6 - (index % 4) * 2));
    ++index;
  }
}

void restore_city_state(city_map &cells, std::span<resources::city_type const> const types, std::span<std::byte const> const packed, uint8_t const stage) {
  /// BBC6 restores into a fresh template; bit-80 beacon templates expand nonzero codes to FF
  if(stage == 1) return;
  validate_size(cells,types,packed.size());
  size_t index{0};
  for(auto &cell : cells) {
    if(!eligible(cell,types)) continue;
    auto const code{(std::to_integer<uint8_t>(packed[index / 4]) >> (6 - (index % 4) * 2)) & 3};
    cell.state = static_cast<uint8_t>((cell.state & 128) ? (code ? 255 : 0) : code << 5);
    ++index;
  }
  std::array<uint8_t,256> limits{};
  for(size_t i{0}; i < types.size(); ++i) limits[i + 1] = types[i].variant_limit;
  assign_city_variants(cells,limits);
}

} // namespace darker::game
