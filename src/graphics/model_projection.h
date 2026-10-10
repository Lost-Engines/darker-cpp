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
  render_geometry::coordinate_bits whole{0};
  render_geometry::fraction fraction{0};
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
  render_geometry::coordinate depth{0};
};

class model_projection {
private:
  struct contribution {
    // Cache complete fixed-point contributions once per bytecode update.
    // Unsigned addition/negation preserves wrapping; transform retains the
    // renderer's coordinate width when it produces the final camera vertex.
    render_geometry::accumulator_bits horizontal{0};
    render_geometry::accumulator_bits depth{0};
    render_geometry::coordinate_bits vertical{0};
  };

  projection_parameters parameters;
  std::array<contribution, 3> contributions{};
  render_geometry::fraction vertical_ab_fraction{0};
  render_geometry::fraction vertical_c_fraction{0};

public:
  explicit model_projection(projection_parameters parameters);
  void set_component(size_t axis, int16_t value);
  void zero_component(size_t axis);
  void negate_component(size_t axis);
  camera_vertex transform() const noexcept;
  projected_vertex project() const;
};

} // namespace darker::graphics
