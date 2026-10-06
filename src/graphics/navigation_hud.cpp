#include "graphics/navigation_hud.h"
#include <bit>
#include <cstddef>
#include <stdexcept>
#include "graphics/cockpit_tables.h"

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

std::optional<radar_pixel> project_radar_contact(world_position const player, std::uint16_t const heading, radar_contact const contact) {
  /// 5AC9–5B55 retain byte-window rejection, signed high products and word truncation
  if(contact.group != radar_group::a && contact.group != radar_group::b) throw std::invalid_argument{"unknown radar contact group"};
  if(contact.hidden || !contact.covered) return std::nullopt;
  auto const relative_x{static_cast<std::uint16_t>(contact.position.x - player.x + 21 * 256)};
  auto const relative_y{static_cast<std::uint16_t>(contact.position.y - player.y + 21 * 256)};
  if((relative_x >> 8) >= 42 || (relative_y >> 8) >= 42) return std::nullopt;
  auto const x{signed_word((relative_x - 21 * 256) * 2)};
  auto const y{signed_word((relative_y - 21 * 256) * 2)};
  unsigned int const angle{static_cast<unsigned int>(heading >> 6)};
  int const sine{original_sine[angle]};
  int const cosine{original_sine[(angle + 256) % 1024]};
  int const pixel_y{signed_word(((x * sine) >> 16) + ((y * cosine) >> 16)) >> 8};
  int const pixel_x{signed_word(((x * cosine) >> 16) - ((y * sine) >> 16)) >> 8};
  int const radius_squared{pixel_x * pixel_x + pixel_y * pixel_y};
  if(radius_squared > 441) return std::nullopt;
  int const base_colour{contact.group == radar_group::a ? 249 : 242};
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

} // namespace darker::graphics
