#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "graphics/flat_polygon.h"
#include "graphics/near_clip.h"
#include "maths/view_basis.h"

namespace darker::graphics {

using projection_axis = maths::view_axis;

struct projection_term {
  std::uint16_t whole{0};
  std::uint8_t fraction{0};
};

struct projection_parameters {
  std::array<projection_axis, 3> axes{};
  projection_term horizontal{};
  projection_term vertical{};
  projection_term depth{};
  screen_vertex origin{};
};

struct projected_vertex {
  screen_vertex screen{};
  std::int16_t depth{0};
};

class model_projection {
private:
  struct contribution {
    projection_term horizontal{};
    projection_term depth{};
    std::uint16_t vertical{0};
  };

  projection_parameters parameters;
  std::array<contribution, 3> contributions{};
  std::uint8_t vertical_ab_fraction{0};
  std::uint8_t vertical_c_fraction{0};

public:
  explicit model_projection(projection_parameters parameters);
  void set_component(std::size_t axis, std::int16_t value);
  void zero_component(std::size_t axis);
  void negate_component(std::size_t axis);
  camera_vertex transform() const noexcept;
  projected_vertex project() const;
};

} // namespace darker::graphics
