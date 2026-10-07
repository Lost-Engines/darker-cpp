#include "graphics/radar_beacons.h"
#include <bit>
#include "game/beacon_light.h"
#include "maths/sine_table.h"

namespace darker::graphics {

void draw_radar_beacons(framework::render::indexed_cockpit_framebuffer &target, game::city_map const &cells,
  world_position const player, uint16_t const heading, game::radar_coverage const &coverage) {
  /// 5898 rotates one lattice origin, then incrementally scans five rows of five tower positions
  auto const signed_word{[](int const value){ return std::bit_cast<int16_t>(static_cast<uint16_t>(value)); }};
  auto const origin{game::beacon_grid_cell({static_cast<uint16_t>(player.x - 18*256),static_cast<uint16_t>(player.y - 18*256)})};
  auto const x{signed_word(player.x - origin[0]*256)}, y{signed_word(player.y - origin[1]*256)};
  auto const index{static_cast<size_t>(heading >> 6)};
  int const sine{maths::original_sine[index]}, cosine{maths::original_sine[(index + 256) % 1024]};
  auto vertical{signed_word(-2*((y*cosine >> 16) + (x*sine >> 16)))};
  auto horizontal{signed_word(-2*((x*cosine >> 16) - (y*sine >> 16)))};
  int const step_sine{0x1200*sine >> 16}, step_cosine{0x1200*cosine >> 16};
  for(unsigned int row{0}; row < 5; ++row) {
    auto bx{vertical}, di{horizontal};
    auto const cell_y{static_cast<uint8_t>(origin[1] + row*9)};
    for(unsigned int column{0}; column < 5; ++column) {
      auto const cell_x{static_cast<uint8_t>(origin[0] + column*9)};
      if(cell_x < 128 && cell_y < 128 && coverage.contains(cell_x,cell_y)) {
        auto const cell{cells[cell_y*128 + cell_x]};
        if(cell.type == 1 && (cell.state & 128)) {
          int const px{di >> 8}, py{bx >> 8};
          int const squared{px*px + py*py};
          if(squared <= 441) target.pixels[(215 + py)*320 + 54 + px] = static_cast<uint8_t>(22 - (squared >> 5));
        }
      }
      bx = signed_word(bx + step_sine);
      di = signed_word(di + step_cosine);
    }
    vertical = signed_word(vertical + step_cosine);
    horizontal = signed_word(horizontal - step_sine);
  }
}

} // namespace darker::graphics
