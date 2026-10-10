#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include "game/city_map.h"
#include "maths/world_coordinates.h"

namespace darker::game {

struct tunnel_segment {
  uint8_t first{0};
  uint8_t second{0};
  uint8_t heights{0};
  uint8_t length{0};
  uint8_t heading{0};
};

struct tunnel_junction {
  uint8_t flags{0};
  std::array<tunnel_segment, 3> edges{};
};

struct tunnel_boundary {
  uint16_t cell{0};
  uint8_t perimeter{0};
  uint8_t height{0};
};

struct tunnel_connection {
  uint16_t cell{0};
  uint8_t route{0};
};

struct tunnel_trace {
  tunnel_connection connection{};
  uint16_t progress{0};
  maths::world_position target{};
};

struct tunnel_start {
  maths::world_position position{};
  uint16_t heading{0};
  uint8_t route{0};
};

class tunnel_network {
private:
  std::array<std::byte, 3200> data{};
  tunnel_segment segment_at(int offset) const;

public:
  explicit tunnel_network(std::span<std::byte const> source);
  std::span<std::byte const> prepared_bytes() const noexcept;
  tunnel_segment segment(uint8_t type, uint8_t route) const;
  tunnel_junction junction(uint8_t type) const;
  tunnel_boundary crossing(uint8_t type, tunnel_connection source) const;
  maths::world_position point(uint8_t type, uint8_t route, uint16_t cell, uint16_t distance) const;
  uint8_t direction(uint8_t type, uint8_t route, uint16_t heading) const;
  tunnel_start start(uint8_t type, uint16_t cell, uint16_t encoded_heading) const;
  tunnel_connection connect(city_map const &cells, tunnel_connection source, uint8_t preferred_heading) const;
  std::optional<tunnel_trace> trace(city_map const &cells, tunnel_connection source,
    maths::world_position position, uint16_t lookahead, uint8_t preferred_heading) const;
  std::optional<tunnel_connection> reacquire(city_map const &cells, tunnel_connection source,
    maths::world_position position, uint8_t preferred_heading) const;
};

} // namespace darker::game
