#include "graphics/radar_beacons.h"
#include <bit>
#include "game/beacon_light.h"
#include "game/random.h"
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

void draw_radar_interference(framework::render::indexed_cockpit_framebuffer &target,
  world_position const player, uint16_t const heading, game::radar_coverage const &coverage, uint16_t &random_state) {
  /// 5941 emits seventeen candidate dots outside radio coverage, consuming the shared 92D2 sequence
  if(coverage.mask == 0x777) return;
  auto const signed_word{[](int const value){ return std::bit_cast<int16_t>(static_cast<uint16_t>(value)); }};
  auto const index{static_cast<size_t>(heading >> 6)};
  int const sine{maths::original_sine[index]}, cosine{maths::original_sine[(index + 256) % 1024]};
  for(unsigned int point{0}; point < 17; ++point) {
    auto const random{game::next_random(random_state)};
    auto const low{static_cast<uint8_t>(random)}, high{static_cast<uint8_t>(random >> 8)};
    int const dx{std::bit_cast<int8_t>(low) >> 3}, dy{std::bit_cast<int8_t>(high) >> 3};
    if(coverage.contains(static_cast<uint8_t>((player.x >> 8) + dx),static_cast<uint8_t>((player.y >> 8) + dy))) continue;
    // Retain the random low bytes during the native byte exchanges, then wrap the doubled words.
    auto const y{signed_word(2*(static_cast<uint8_t>(dy)*256 + low))};
    auto const x{signed_word(2*(static_cast<uint8_t>(dx)*256 + high))};
    int const px{signed_word((x*cosine >> 16) - (y*sine >> 16)) >> 8};
    int const py{signed_word((y*cosine >> 16) + (x*sine >> 16)) >> 8};
    int const squared{px*px + py*py};
    if(squared <= 441) target.pixels[(215+py)*320 + 54+px] = static_cast<uint8_t>(22 - (squared >> 5));
  }
}

} // namespace darker::graphics
