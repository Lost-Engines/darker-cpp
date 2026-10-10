#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace darker::resources {

struct city_type {
  static uint8_t constexpr background_marker{0xff};                            // non-colliding geometry drawn before ordinary objects

  uint16_t model_offset{0};
  uint8_t column_fraction{0};
  uint8_t row_fraction{0};
  uint8_t collision_marker{0};
  uint8_t unknown_5{0};
  uint8_t variant_limit{0};
  uint8_t unknown_7{0};
};

struct model_header {
  uint8_t point_distance{0};
  uint8_t point_colour{0};
  uint8_t flat_distance{0};
  int16_t height{0};
  uint16_t extent{0};
};

class geometry_bank {
private:
  std::vector<std::byte> data;
  std::vector<city_type> types;
  std::vector<uint16_t> specials;
  size_t pool_start{0};
  size_t pool_size{0};

  void check_model(size_t offset) const;

public:
  explicit geometry_bank(std::vector<std::byte> resource);
  std::span<city_type const> city_types() const noexcept;
  std::span<uint16_t const> special_models() const noexcept;
  std::span<std::byte const> model_pool() const noexcept;
  std::span<std::byte const> world_data() const noexcept;
  model_header header_at(size_t offset) const;
  size_t city_model_offset(unsigned int type, uint8_t state, uint8_t damage_mask) const;
};

} // namespace darker::resources
