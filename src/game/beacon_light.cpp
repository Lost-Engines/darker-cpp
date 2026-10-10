#include "game/beacon_light.h"
#include <bit>
#include <stdexcept>
#include "maths/world_coordinates.h"

namespace darker::game {
namespace {

constexpr auto beacon_lookup() {
  /// Reproduce 0396's wrapping nine-cell lookup, including the out-of-map sentinel region
  std::array<uint8_t, 256> result{};
  uint8_t constexpr sentinel_cell{0xbd};
  uint8_t constexpr initial_cell{0xb8};
  uint8_t index{sentinel_cell};
  uint8_t value{initial_cell};
  do {
    value = static_cast<uint8_t>(value + beacon_spacing_cells);
    for(int count{0}; count < beacon_spacing_cells; ++count) result[index++] = value;
  } while(value != sentinel_cell);
  return result;
}

auto constexpr lookup{beacon_lookup()};

int16_t signed_word(int const value) {
  /// Wrap before interpreting the original signed coordinate products
  return std::bit_cast<int16_t>(static_cast<uint16_t>(value));
}

} // anonymous namespace

std::array<uint8_t, 2> beacon_grid_cell(maths::map_position const position) noexcept {
  /// Share the original nine-cell lookup with charging, coordinates and tower radar sampling
  return {lookup[position.column >> 8], lookup[position.row >> 8]};
}

std::array<uint8_t, 2> beacon_grid_coordinates(maths::map_position const position) noexcept {
  /// 5C65/573C map both player coordinates to the nearest beacon and blank the pair outside coverage
  auto const column{lookup[position.column >> 8]}, row{lookup[position.row >> 8]};
  uint8_t constexpr outside_map_bit{0x80};
  if((column | row) & outside_map_bit) return {};
  return {static_cast<uint8_t>(column + 1), static_cast<uint8_t>(row + 1)};
}

uint16_t beacon_light(std::span<city_cell const, city_map_cell_count> const cells,
  maths::world_position const position, maths::map_fractions const fractions) {
  /// 8450 samples one lattice cell of type 1, then applies its mutable strength to fixed-point distance attenuation
  int constexpr maximum_lit_height{0x2d60};
  int constexpr beacon_model_type{1};
  int constexpr horizontal_distance_scale{16};
  int constexpr scaled_cell_centre{2048};
  int constexpr vertical_distance_scale{2};
  int constexpr scaled_beacon_height{4800};
  int constexpr squared_distance_shift{14};
  int constexpr minimum_distance_denominator{256};
  int constexpr distance_denominator_bias{1024};
  unsigned int constexpr light_strength_multiplier{0x10101u};
  unsigned int constexpr maximum_light_result{0xffffu};
  if(signed_word(position.height) >= maximum_lit_height) return 0;
  auto const x{lookup[position.column >> 8]};
  auto const y{lookup[position.row >> 8]};
  if(x >= city_map_size.column || y >= city_map_size.row) return 0;
  auto const cell{cells[city_cell_index(x, y)]};
  if(cell.type != beacon_model_type) return 0;
  auto const dx{signed_word((position.column - x * 256) * horizontal_distance_scale + (fractions.column >> 4) - scaled_cell_centre)};
  auto const dy{signed_word((position.row - y * 256) * horizontal_distance_scale + (fractions.row >> 4) - scaled_cell_centre)};
  auto const dz{signed_word(position.height * vertical_distance_scale - scaled_beacon_height)};
  auto const squared{static_cast<uint32_t>(dx * dx) + static_cast<uint32_t>(dy * dy) + static_cast<uint32_t>(dz * dz)};
  auto denominator{static_cast<uint16_t>(squared >> squared_distance_shift)};
  if(denominator < minimum_distance_denominator) denominator = minimum_distance_denominator;
  denominator = static_cast<uint16_t>(denominator + distance_denominator_bias);
  auto const numerator{cell.state * light_strength_multiplier};
  if(denominator == 0 || numerator / denominator > maximum_light_result) {
    throw std::domain_error{"Beacon distance produces an original 16-bit division fault"};
  }
  return static_cast<uint16_t>(numerator / denominator);
}

} // namespace darker::game
