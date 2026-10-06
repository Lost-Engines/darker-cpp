#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include "graphics/model_projection.h"
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

struct model_colours {
  std::array<std::uint8_t, 28> shades{};
  std::uint8_t dynamic{0};
};

void draw_flat_model(framework::render::indexed_cockpit_framebuffer &target, std::span<std::byte const> pool,
  std::size_t model_offset, projection_parameters projection, model_colours const &colours);

} // namespace darker::graphics
