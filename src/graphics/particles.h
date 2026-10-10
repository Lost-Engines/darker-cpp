#pragma once

#include <vector>
#include "game/effects.h"
#include "graphics/camera.h"
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

struct particle_point {
  int16_t x{0};
  int16_t y{0};
  int16_t depth{0};
  bool operator==(particle_point const &) const = default;
};

std::vector<particle_point> project_emitter(game::particle_emitter const &emitter, model_placement centre,
  camera_basis const &basis, screen_vertex const &origin);
void draw_particle(framework::render::indexed_cockpit_framebuffer &target,
  framework::render::indexed_cockpit_framebuffer const &sheet, particle_point point, uint8_t phase, int bottom);

} // namespace darker::graphics
