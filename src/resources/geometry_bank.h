#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace darker::resources {

struct city_type {
  static std::uint8_t constexpr background_marker{0xff}; // non-colliding geometry drawn before ordinary objects

  std::uint16_t model_offset{0};
  std::uint8_t column_fraction{0};
  std::uint8_t row_fraction{0};
  std::uint8_t collision_marker{0};
  std::uint8_t unknown_5{0};
  std::uint8_t variant_limit{0};
  std::uint8_t unknown_7{0};
};

struct model_header {
  std::uint8_t point_distance{0};
  std::uint8_t point_colour{0};
  std::uint8_t flat_distance{0};
  std::int16_t height{0};
  std::uint16_t extent{0};
};

class geometry_bank {
private:
  std::vector<std::byte> data;
  std::vector<city_type> types;
  std::vector<std::uint16_t> specials;
  std::size_t pool_start{0};
  std::size_t pool_size{0};

  void check_model(std::size_t offset) const;

public:
  explicit geometry_bank(std::vector<std::byte> resource);
  std::span<city_type const> city_types() const noexcept;
  std::span<std::uint16_t const> special_models() const noexcept;
  std::span<std::byte const> model_pool() const noexcept;
  std::span<std::byte const> world_data() const noexcept;
  model_header header_at(std::size_t offset) const;
  std::size_t city_model_offset(unsigned int type, std::uint8_t state, std::uint8_t damage_mask) const;
};

} // namespace darker::resources
