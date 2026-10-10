#include "resources/geometry_bank.h"
#include <bit>
#include <stdexcept>
#include <utility>

namespace darker::resources {
namespace {

std::uint8_t byte(std::span<std::byte const> const data, std::size_t const offset) {
  /// Reject truncated directory fields before accessing their bytes
  if(offset >= data.size()) throw std::invalid_argument{"Truncated geometry bank"};
  return std::to_integer<std::uint8_t>(data[offset]);
}

std::uint16_t word(std::span<std::byte const> const data, std::size_t const offset) {
  /// Decode little-endian fields without alignment or host-endian assumptions
  return static_cast<std::uint16_t>(byte(data, offset) | byte(data, offset + 1) << 8);
}

} // namespace

geometry_bank::geometry_bank(std::vector<std::byte> resource) : data{std::move(resource)} {
  /// Own the original bank bytes and decode its city/special directories without relocating model offsets
  auto const count{byte(data, 0)};
  std::size_t cursor{1};
  types.reserve(count);
  for(unsigned int i{0}; i < count; ++i, cursor += 8) {
    types.push_back({
      .model_offset{word(data, cursor)},
      .column_fraction{byte(data, cursor + 2)},
      .row_fraction{byte(data, cursor + 3)},
      .collision_marker{byte(data, cursor + 4)},
      .unknown_5{byte(data, cursor + 5)},
      .variant_limit{byte(data, cursor + 6)},
      .unknown_7{byte(data, cursor + 7)},
    });
  }
  auto const special_count{byte(data, cursor++)};
  specials.reserve(special_count);
  for(unsigned int i{0}; i < special_count; ++i, cursor += 2) specials.push_back(word(data, cursor));
  pool_size = word(data, cursor);
  pool_start = cursor + 2;
  if(pool_size > data.size() - pool_start) throw std::invalid_argument{"Geometry pool exceeds its resource"};
  for(auto const &type : types) check_model(type.model_offset);
  for(auto const offset : specials) check_model(offset);
}

void geometry_bank::check_model(std::size_t const offset) const {
  /// A definition needs its complete eleven-byte header within the pool
  if(offset > pool_size || pool_size - offset < 11) throw std::invalid_argument{"Model header lies outside its geometry pool"};
}

std::span<city_type const> geometry_bank::city_types() const noexcept {
  /// Return one-based game type records in their original directory order
  return types;
}

std::span<std::uint16_t const> geometry_bank::special_models() const noexcept {
  /// Special slots are zero-based and may share model offsets
  return specials;
}

std::span<std::byte const> geometry_bank::model_pool() const noexcept {
  /// The bytecode view borrows storage owned by this bank
  return std::span{data}.subspan(pool_start, pool_size);
}

std::span<std::byte const> geometry_bank::world_data() const noexcept {
  /// Preserve the trailing world/collision data for its separate consumers
  return std::span{data}.subspan(pool_start + pool_size);
}

model_header geometry_bank::header_at(std::size_t const offset) const {
  /// Decode the drawing metadata shared by city and special models, leaving linked-state traversal separate
  check_model(offset);
  auto const pool{model_pool()};
  return {
    .point_distance{byte(pool, offset + 4)},
    .point_colour{byte(pool, offset + 5)},
    .flat_distance{byte(pool, offset + 6)},
    .height{std::bit_cast<std::int16_t>(word(pool, offset + 7))},
    .extent{word(pool, offset + 9)},
  };
}

std::size_t geometry_bank::city_model_offset(unsigned int const type, std::uint8_t const state, std::uint8_t const damage_mask) const {
  /// 2DE1 follows the alternate link first, then the bank-masked number of damage links
  if(type == 0 || type > types.size()) throw std::out_of_range{"City type is outside the geometry directory"};
  if(damage_mask != 0x20 && damage_mask != 0x60) throw std::invalid_argument{"Unsupported city damage-state mask"};
  std::size_t offset{types[type - 1].model_offset};
  auto const follow{[&](std::size_t const field){
    auto const delta{std::bit_cast<std::int16_t>(word(model_pool(), offset + field))};
    auto const next{static_cast<std::ptrdiff_t>(offset) + delta};
    if(next < 0) throw std::invalid_argument{"City model link precedes its geometry pool"};
    offset = static_cast<std::size_t>(next);
    check_model(offset);
  }};
  if(state & 0x80) follow(2);
  for(unsigned int i{0}; i < static_cast<unsigned int>((state & damage_mask) >> 5); ++i) follow(0);
  return offset;
}

} // namespace darker::resources
