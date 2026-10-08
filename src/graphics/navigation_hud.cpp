#include "graphics/navigation_hud.h"
#include <algorithm>
#include <bit>
#include <cstddef>
#include <stdexcept>
#include "graphics/bitmap_hud.h"
#include "graphics/cockpit_tables.h"
#include "graphics/procedural_hud.h"
#include "maths/sine_table.h"

namespace darker::graphics {
namespace {

std::int16_t signed_word(int const value) noexcept {
  /// Preserve original 16-bit wrapping before interpreting a signed intermediate
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

} // namespace

std::uint8_t compass_phase(std::uint16_t const heading) noexcept {
  /// 56C7–56CF rotates the heading high byte and quantises it to 136 marker states
  auto const rotated{static_cast<std::uint8_t>((heading >> 8) + 64)};
  return static_cast<std::uint8_t>((rotated * 136) >> 8);
}

std::array<pixel_position, 5> compass_points(std::uint8_t const phase) {
  /// 5395 folds the phase into the original paired-byte arc and reflects each coordinate
  if(phase >= 136) throw std::invalid_argument{"compass phase must be below 136"};
  unsigned int index{phase};
  int const y_sign{index < 68 ? -1 : 1};
  int x_sign{y_sign};
  if(index >= 68) index -= 68;
  if(index >= 34) {
    index = 67 - index;
    x_sign = -x_sign;
  }
  std::array<pixel_position, 5> result;
  for(std::size_t i{0}; i < result.size(); ++i) {
    auto const offset{compass_offsets[index + i]};
    result[i] = {.x{54 + x_sign * offset.x}, .y{215 + y_sign * offset.y}};
  }
  return result;
}

void update_compass(framework::render::indexed_cockpit_framebuffer &target, std::uint8_t const previous, std::uint8_t const current) {
  /// 5384 erases five old pixels with zero, then draws the new five-colour marker
  auto const old_points{compass_points(previous)};
  auto const new_points{compass_points(current)};
  std::array<std::uint8_t, 5> constexpr colours{0x9e, 0x9c, 0x9c, 0x9c, 0x9e};
  for(auto const point : old_points) target.pixels[static_cast<std::size_t>(point.y * 320 + point.x)] = 0;
  for(std::size_t i{0}; i < new_points.size(); ++i) {
    auto const point{new_points[i]};
    target.pixels[static_cast<std::size_t>(point.y * 320 + point.x)] = colours[i];
  }
}

std::optional<radar_pixel> project_radar_contact(world_position const player, std::uint16_t const heading, radar_contact const contact, radar_scale const scale) {
  /// 5AC9–5B55 retain byte-window rejection, signed high products and word truncation
  if(contact.group != radar_group::a && contact.group != radar_group::b && contact.group != radar_group::underground) throw std::invalid_argument{"unknown radar contact group"};
  if(scale != radar_scale::normal && scale != radar_scale::enlarged) throw std::invalid_argument{"unknown radar scale"};
  if(contact.hidden || !contact.covered) return std::nullopt;
  auto const relative_x{static_cast<std::uint16_t>(contact.position.x - player.x + 21 * 256)};
  auto const relative_y{static_cast<std::uint16_t>(contact.position.y - player.y + 21 * 256)};
  if((relative_x >> 8) >= 42 || (relative_y >> 8) >= 42) return std::nullopt;
  auto const x{signed_word((relative_x - 21 * 256) * 2)};
  auto const y{signed_word((relative_y - 21 * 256) * 2)};
  unsigned int const angle{static_cast<unsigned int>(heading >> 6)};
  int const sine{maths::original_sine[angle]};
  int const cosine{maths::original_sine[(angle + 256) % 1024]};
  int const multiplier{scale == radar_scale::enlarged ? 3 : 1};
  int const pixel_y{signed_word(signed_word(((x * sine) >> 16) + ((y * cosine) >> 16)) * multiplier) >> 8};
  int const pixel_x{signed_word(signed_word(((x * cosine) >> 16) - ((y * sine) >> 16)) * multiplier) >> 8};
  int const radius_squared{pixel_x * pixel_x + pixel_y * pixel_y};
  if(scale == radar_scale::enlarged) {
    if(radius_squared >= 3965) return std::nullopt;
    int const shift{contact.group == radar_group::a ? 8 : 9};
    return radar_pixel{
      .position{.x{73 + pixel_x}, .y{105 + pixel_y}},
      .colour{static_cast<std::uint8_t>(7 - ((radius_squared - 3965) >> shift))},
    };
  }
  if(radius_squared > 441) return std::nullopt;
  int const base_colour{contact.group == radar_group::underground ? 22 : contact.group == radar_group::a ? 249 : 242};
  return radar_pixel{
    .position{.x{54 + pixel_x}, .y{215 + pixel_y}},
    .colour{static_cast<std::uint8_t>(base_colour - (radius_squared >> 5))},
  };
}

void draw_radar_contacts(framework::render::indexed_cockpit_framebuffer &target,
  world_position const player, std::uint16_t const heading, std::span<radar_contact const> const contacts) {
  /// Preserve supplied list order; coverage and allegiance decisions belong to game logic
  for(auto const &contact : contacts) {
    if(auto const pixel{project_radar_contact(player, heading, contact)}) {
      target.pixels[static_cast<std::size_t>(pixel->position.y * 320 + pixel->position.x)] = pixel->colour;
    }
  }
}

namespace {

void draw_contact_symbol(framework::render::indexed_cockpit_framebuffer &target, pixel_position const anchor, std::uint8_t const colour) {
  /// 5BE2 emits a twelve-pixel rounded diamond at anchor offsets one through four
  for(int y{1}; y <= 4; ++y) {
    bool const narrow{y == 1 || y == 4};
    for(int x{narrow ? 2 : 1}; x <= (narrow ? 3 : 4); ++x) {
      int const px{anchor.x + x};
      int const py{anchor.y + y};
      if(px >= 0 && px < 320 && py >= 0 && py < 240) target.pixels[static_cast<std::size_t>(py * 320 + px)] = colour;
    }
  }
}

} // namespace

void draw_navigation_contact(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target, pixel_position const destination,
  std::uint8_t const height, std::uint8_t const reference_height) {
  /// 5B92 restores an alignment-specific mask, then colours the glyph by signed byte height difference
  auto const difference{std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(height - reference_height))};
  int const half{difference >> 1};
  int const magnitude{std::min(10, half < 0 ? ~half : half)};
  auto const colour{static_cast<std::uint8_t>((half < 0 ? 183 : 151) - magnitude)};
  copy_mask(cache.pixels, target.pixels, {.x{navigation_source_x[destination.x & 3]}, .y{8}}, destination, navigation_mask);
  draw_contact_symbol(target, destination, colour);
}

void draw_enlarged_radar_surround(framework::render::indexed_cockpit_framebuffer &target, std::uint16_t const heading) {
  /// A5D6/A712 construct exclusive-right spans symmetric about two central scanlines
  std::array<int, 63> half_widths{};
  int x{63};
  int y{0};
  int error{31};
  do {
    do {
      half_widths[y++] = x;
      error -= y;
    } while(error >= 0);
    error += x--;
    half_widths[x] = y;
  } while(y < x);
  std::fill_n(target.pixels.begin() + 40 * 320 + 56, 40, 8);
  for(int row{0}; row < 63; ++row) {
    int const width{half_widths[row]};
    for(int const destination : {107 - row, 108 + row}) {
      std::fill_n(target.pixels.begin() + destination * 320 + 76 - width, width * 2, 8);
    }
  }
  draw_contact_symbol(target, {.x{74}, .y{105}}, 139);
  unsigned int const angle{static_cast<unsigned int>(heading >> 6)};
  pixel_position const end{
    .x{76 + (maths::original_sine[angle] >> 10)},
    .y{108 - (maths::original_sine[(angle + 256) % 1024] >> 10)},
  };
  draw_screen_line(target, {.x{76}, .y{108}}, end, 139);
}

void draw_enlarged_radar(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target, radar_view_state const view, std::span<radar_contact const> const contacts) {
  /// Assemble the normal Caero mono enlarged view without changing the underlying cockpit surface
  draw_enlarged_radar_surround(target, view.heading);
  for(auto const &contact : contacts) {
    if(auto const pixel{project_radar_contact(view.player, view.heading, contact, radar_scale::enlarged)}) {
      draw_contact_symbol(target, pixel->position, pixel->colour);
    }
  }
  draw_grid_coordinate(cache, target, {.x{56}, .y{41}}, view.row, coordinate_font::large);
  copy_rectangle(cache.pixels, target.pixels, {.x{312}, .y{85}}, {.x{72}, .y{41}}, 8, 7);
  draw_grid_coordinate(cache, target, {.x{80}, .y{41}}, view.column, coordinate_font::large);
}

} // namespace darker::graphics
