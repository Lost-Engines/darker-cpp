#include "graphics/bitmap_hud.h"
#include <array>
#include <stdexcept>
#include "graphics/blit.h"
#include "graphics/cockpit_tables.h"
#include "maths/sine_table.h"

namespace darker::graphics {

void draw_weapon_icon(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target, weapon_icon_slot const slot, std::uint8_t const selection) {
  /// Selection IDs include the automatic Dual Launch and Diffuser stages, not just number keys
  bool const primary{slot == weapon_icon_slot::primary};
  if(!primary && slot != weapon_icon_slot::secondary) throw std::invalid_argument{"unknown weapon icon slot"};
  std::array<bool, 11> constexpr primary_ids{true, true, true, true, false, false, false, true, false, false, false};
  if(selection >= primary_ids.size() || (selection != 0 && primary_ids[selection] != primary)) {
    throw std::invalid_argument{"weapon selection is not valid for this icon slot"};
  }
  pixel_position const destination{.x{primary ? 260 : 268}, .y{195}};
  // Zero is an empty selection: restore the panel instead of sampling X=140.
  pixel_position const source{selection == 0 ? destination : pixel_position{.x{140 + selection * 8}, .y{8}}};
  copy_rectangle(cache.pixels, target.pixels, source, destination, 8, 12);
}

void draw_grid_coordinate(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target, pixel_position const destination,
  std::uint8_t const encoded_coordinate, coordinate_font const font) {
  /// 5429/5472 share number conversion, but large invalid coordinates use blank glyphs
  if(font != coordinate_font::small && font != coordinate_font::large) throw std::invalid_argument{"unknown coordinate font"};
  bool const large{font == coordinate_font::large};
  bool const valid{encoded_coordinate > 0 && encoded_coordinate < 128};
  if(!valid && !large) {
    copy_rectangle(cache.pixels, target.pixels, destination, destination, 8, 5);
    return;
  }
  int const width{large ? 8 : 4};
  int const height{large ? 7 : 5};
  int const source_x{large ? 312 : 308};
  int const number{valid ? (encoded_coordinate - 1) / 9 + 1 : 0};
  std::array<int, 2> const digits{valid ? number / 10 : 10, valid ? number % 10 : 10};
  for(int i{0}; i < 2; ++i) {
    copy_rectangle(cache.pixels, target.pixels, {.x{source_x}, .y{8 + digits[i] * height}},
      {.x{destination.x + i * width}, .y{destination.y}}, width, height);
  }
}

void update_caero_bitmaps(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target, caero_bitmap_state const previous, caero_bitmap_state const current) {
  /// Match the callback directory's changed-field dispatch; row is left, column is right
  if(previous.row != current.row) draw_grid_coordinate(cache, target, {.x{44}, .y{185}}, current.row);
  if(previous.column != current.column) draw_grid_coordinate(cache, target, {.x{56}, .y{185}}, current.column);
  if(previous.primary_weapon != current.primary_weapon) draw_weapon_icon(cache, target, weapon_icon_slot::primary, current.primary_weapon);
  if(previous.secondary_weapon != current.secondary_weapon) draw_weapon_icon(cache, target, weapon_icon_slot::secondary, current.secondary_weapon);
}

void update_skimma_bitmaps(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target, craft const type, skimma_bitmap_state const previous, skimma_bitmap_state const current) {
  /// 520A restores the previous bearing mask; 52AB selects one of four sources per weapon
  if(type != craft::skimma && type != craft::upgraded_skimma) throw std::invalid_argument{"Skimma bitmap state requires a Skimma"};
  if(previous.bearing > 7 || current.bearing > 7) throw std::invalid_argument{"bearing symbol must be zero through seven"};
  for(auto const &state : {previous, current}) {
    for(auto const weapon : state.weapons) {
      if(weapon > 3) throw std::invalid_argument{"Skimma weapon display state must be zero through three"};
    }
    if(type == craft::skimma && state.weapons[2] != 0) throw std::invalid_argument{"ordinary Skimma has only two weapon displays"};
  }
  auto const draw_strip{[&](hud_strip const &strip, pixel_position const source, pixel_position const destination){
    copy_mask(cache.pixels, target.pixels, {.x{source.x}, .y{source.y + strip.y_offset}},
      {.x{destination.x}, .y{destination.y + strip.y_offset}}, strip.rows);
  }};
  if(previous.bearing != current.bearing) {
    if(previous.bearing != 0) draw_strip(bearing_strips[previous.bearing - 1], bearing_destination, bearing_destination);
    if(current.bearing != 0) draw_strip(bearing_strips[current.bearing - 1], {.x{8}, .y{32}}, bearing_destination);
  }
  std::size_t const weapon_count{type == craft::skimma ? 2u : 3u};
  for(std::size_t i{0}; i < weapon_count; ++i) {
    if(previous.weapons[i] != current.weapons[i]) draw_strip(weapon_status_strips[i], weapon_status_sources[current.weapons[i]], weapon_status_destination);
  }
}

void draw_skimma_weapon_ring(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target, craft const type,
  std::uint8_t const weapon, std::uint8_t const radius, std::uint8_t const remaining) {
  /// 5D83 uses signed sine high bytes and alignment-specific remaining/spent artwork
  if(type != craft::skimma && type != craft::upgraded_skimma) throw std::invalid_argument{"weapon ring requires a Skimma"};
  if(weapon >= (type == craft::skimma ? 2 : 3)) throw std::invalid_argument{"weapon ring index outside craft capacity"};
  if(radius < 15 || radius > 127 || remaining > ring_capacities[weapon]) throw std::invalid_argument{"weapon ring inputs outside supported gameplay range"};
  int count{remaining};
  for(unsigned int offset{1}; offset < 1920; offset += ring_steps[weapon]) {
    offset |= 1;
    int const sine{maths::original_sine[offset / 2] >> 8};
    int const cosine{maths::original_sine[((offset + 512) % 2048) / 2] >> 8};
    pixel_position const destination{.x{158 + ((sine * radius) >> 8)}, .y{88 - ((cosine * radius) >> 8)}};
    --count;
    copy_mask(cache.pixels, target.pixels, {.x{8 + 5 * (destination.x & 3)}, .y{count >= 0 ? 47 : 52}}, destination, ring_mask);
  }
}

} // namespace darker::graphics
