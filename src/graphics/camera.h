#pragma once

#include <array>
#include <cstdint>
#include "graphics/model_projection.h"

namespace darker::graphics {

using camera_angles = maths::view_angles;

struct camera_position {
  std::uint16_t column{0};                                                      // 1/1024 cell, matching the patched origin subtractors
  std::uint16_t row{0};
  std::int16_t altitude{0};
};

struct model_origin {
  std::uint16_t column{0};                                                      // 1/256 cell, including the type record's fractional placement
  std::uint16_t row{0};
  std::int16_t height{0};                                                       // selected header height minus the type's vertical placement
};

struct model_placement {
  projection_term horizontal{};
  projection_term vertical{};
  projection_term depth{};
  std::uint16_t sorting_distance{0};
};

using camera_basis = maths::view_basis;                             // model components A, B and C; map column/row use B and -A

camera_basis orient_model(camera_basis const &camera, camera_angles angles) noexcept;

camera_basis make_camera_basis(camera_angles angles) noexcept;
model_placement place_model(camera_basis const &basis, camera_position camera, model_origin origin) noexcept;

} // namespace darker::graphics
