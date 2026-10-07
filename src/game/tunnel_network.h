#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace darker::game {

struct tunnel_segment {
  uint8_t first{0};
  uint8_t second{0};
  uint8_t heights{0};
  uint8_t length{0};
  uint8_t heading{0};
};

struct tunnel_start {
  std::array<uint16_t,3> position{};
  uint16_t heading{0};
  uint8_t route{0};
};

class tunnel_network {
private:
  std::array<std::byte,3200> data{};

public:
  explicit tunnel_network(std::span<std::byte const> source);
  std::span<std::byte const> prepared_bytes() const noexcept;
  tunnel_segment segment(uint8_t type, uint8_t route) const;
  std::array<uint16_t,3> point(uint8_t type, uint8_t route, uint16_t cell, uint16_t distance) const;
  uint8_t direction(uint8_t type, uint8_t route, uint16_t heading) const;
  tunnel_start start(uint8_t type, uint16_t cell, uint16_t encoded_heading) const;
};

} // namespace darker::game
