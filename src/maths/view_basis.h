#pragma once

#include <array>
#include <cstdint>

namespace darker::maths {

struct attitude_angles {
  uint16_t heading{0};
  uint16_t pitch{0};
  uint16_t roll{0};

  bool operator==(attitude_angles const &) const = default;
};

struct view_axis {
  int16_t horizontal{0};
  int16_t vertical{0};
  int16_t depth{0};
};

using view_basis = std::array<view_axis, 3>;
view_basis make_view_basis(attitude_angles angles) noexcept;

} // namespace darker::maths
