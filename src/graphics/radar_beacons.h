#pragma once

#include "game/radar_coverage.h"
#include "graphics/navigation_hud.h"

namespace darker::graphics {

void draw_radar_beacons(framework::render::indexed_cockpit_framebuffer &target, game::city_map const &cells,
  world_position player, uint16_t heading, game::radar_coverage const &coverage);

void draw_radar_interference(framework::render::indexed_cockpit_framebuffer &target,
  world_position player, uint16_t heading, game::radar_coverage const &coverage, uint16_t &random_state);

} // namespace darker::graphics
