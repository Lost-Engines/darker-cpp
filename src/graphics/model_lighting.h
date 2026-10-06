#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include "graphics/model_renderer.h"

namespace darker::graphics {

class distance_shading {
private:
  std::vector<std::array<std::uint8_t, 28>> tables;

public:
  explicit distance_shading(unsigned int count = 60);
  model_colours colours(std::uint16_t depth, model_path path, std::uint8_t light) const;
};

} // namespace darker::graphics
