#include "graphics/radar_beacons.h"
#include <bit>
#include "game/beacon_light.h"
#include "game/random.h"
#include "maths/angle.h"

namespace darker::graphics {

void draw_radar_beacons(framework::render::indexed_cockpit_framebuffer &target, game::city_map const &cells,
  world_position const &player, uint16_t const heading, game::radar_coverage const &coverage) {
  /// 5898 rotates one lattice origin, then incrementally scans five rows of five tower positions
  auto const signed_word{[](int const value){
    return std::bit_cast<int16_t>(static_cast<uint16_t>(value));
  }};
  int constexpr scan_radius_cells{18};
  unsigned int constexpr scan_width_beacons{5};
  int constexpr projected_beacon_spacing{0x1200};
  int constexpr beacon_model_type{1};
  int constexpr lit_beacon_flag{0x80};
  auto const origin{game::beacon_grid_cell({static_cast<uint16_t>(player.x - scan_radius_cells * 256), static_cast<uint16_t>(player.y - scan_radius_cells * 256)})};
  auto const x{signed_word(player.x - origin[0] * 256)}, y{signed_word(player.y - origin[1] * 256)};
  auto const index{static_cast<unsigned int>(heading >> 6)};
  int const sine{maths::original_sine[index]}, cosine{maths::phase_cosine(index)};
  auto vertical{signed_word(-2 * ((y * cosine >> 16) + (x * sine >> 16)))};
  auto horizontal{signed_word(-2 * ((x * cosine >> 16) - (y * sine >> 16)))};
  int const step_sine{projected_beacon_spacing * sine >> 16}, step_cosine{projected_beacon_spacing * cosine >> 16};
  for(unsigned int row{0}; row < scan_width_beacons; ++row) {
    auto bx{vertical}, di{horizontal};
    auto const cell_y{static_cast<uint8_t>(origin[1] + row * game::beacon_spacing_cells)};
    for(unsigned int column{0}; column < scan_width_beacons; ++column) {
      auto const cell_x{static_cast<uint8_t>(origin[0] + column * game::beacon_spacing_cells)};
      if(cell_x < game::city_map_size.column && cell_y < game::city_map_size.row && coverage.contains(cell_x, cell_y)) {
        auto const cell{cells[game::city_cell_index(cell_x, cell_y)]};
        if(cell.type == beacon_model_type && (cell.state & lit_beacon_flag)) {
          int const px{di >> 8}, py{bx >> 8};
          int const squared{vec2<int>{px, py}.length_sq()};
          if(squared <= radar_layout::radius_squared) target.pixels[(radar_layout::centre.y + py) * target.width + radar_layout::centre.x + px] = static_cast<uint8_t>(radar_layout::grey_centre_colour - (squared >> radar_layout::brightness_distance_shift));
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
  world_position const &player, uint16_t const heading, game::radar_coverage const &coverage, uint16_t &random_state) {
  /// 5941 emits seventeen candidate dots outside radio coverage, consuming the shared 92D2 sequence
  unsigned int constexpr interference_candidates_per_frame{17};
  if(coverage.mask == game::radar_coverage::full_coverage_mask) return;
  auto const signed_word{[](int const value){
    return std::bit_cast<int16_t>(static_cast<uint16_t>(value));
  }};
  auto const index{static_cast<unsigned int>(heading >> 6)};
  int const sine{maths::original_sine[index]}, cosine{maths::phase_cosine(index)};
  for(unsigned int point{0}; point < interference_candidates_per_frame; ++point) {
    auto const random{game::next_random(random_state)};
    auto const low{static_cast<uint8_t>(random)}, high{static_cast<uint8_t>(random >> 8)};
    int const dx{std::bit_cast<int8_t>(low) >> 3}, dy{std::bit_cast<int8_t>(high) >> 3};
    if(coverage.contains(static_cast<uint8_t>((player.x >> 8) + dx), static_cast<uint8_t>((player.y >> 8) + dy))) continue;
    // retain the random low bytes during the native byte exchanges, then wrap the doubled words
    auto const y{signed_word(2 * (static_cast<uint8_t>(dy) * 256 + low))};
    auto const x{signed_word(2 * (static_cast<uint8_t>(dx) * 256 + high))};
    int const px{signed_word((x * cosine >> 16) - (y * sine >> 16)) >> 8};
    int const py{signed_word((y * cosine >> 16) + (x * sine >> 16)) >> 8};
    int const squared{vec2<int>{px, py}.length_sq()};
    if(squared <= radar_layout::radius_squared) target.pixels[(radar_layout::centre.y + py) * target.width + radar_layout::centre.x + px] = static_cast<uint8_t>(radar_layout::grey_centre_colour - (squared >> radar_layout::brightness_distance_shift));
  }
}

} // namespace darker::graphics
