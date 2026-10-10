#include "graphics/procedural_hud.h"
#include <algorithm>
#include <bit>
#include <cstddef>
#include <span>
#include <stdexcept>
#include "graphics/cockpit_tables.h"
#include "maths/sine_table.h"

namespace darker::graphics {
namespace {

void put_pixel(framework::render::indexed_cockpit_framebuffer &target, int const x, int const y, std::uint8_t const colour) noexcept {
  /// Keep host writes within the physical surface
  if(x >= 0 && x < 320 && y >= 0 && y < 240) target.pixels[static_cast<std::size_t>(y * 320 + x)] = colour;
}

std::size_t draw_outline(framework::render::indexed_cockpit_framebuffer &target,
  std::span<std::uint8_t const> const stream, pixel_position const &left_origin, pixel_position const &right_origin,
  int const left_step, int const right_step, std::uint8_t colour) {
  auto left{left_origin};
  auto right{right_origin};
  /// Decode the two independently advancing pixel paths shared by markers and fixed HUD outlines
  for(std::size_t cursor{0}; cursor < stream.size(); ++cursor) {
    put_pixel(target, left.x, left.y, colour);
    put_pixel(target, right.x, right.y, colour);
    auto const command{stream[cursor]};
    if(command & 1) {
      --left.x;
      ++right.x;
    }
    if(command & 2) {
      left.y += left_step;
      right.y += right_step;
    }
    if((command & 2) && (command & 0xfc) == 0x80) return cursor + 1;
    colour = static_cast<std::uint8_t>(colour + (std::bit_cast<std::int8_t>(command) >> 2));
  }
  throw std::logic_error{"unterminated HUD outline stream"};
}

} // namespace

void draw_aircraft_threats(framework::render::indexed_cockpit_framebuffer &target,
  resources::font_resource const &font, std::array<std::uint8_t,4> const &errors) {
  /// 574F maps aim errors to brightness; 45DD draws compact-font glyph 81h, strongest on the left
  for(std::size_t i{0}; i < errors.size(); ++i) {
    int const x{232 - static_cast<int>(i) * 8};
    if(errors[i] < 64) {
      auto const level{static_cast<std::uint8_t>(14 - ((errors[i] * 56) >> 8))};
      draw_glyph(target,font,resources::font_face::compact,0x81,{x, 180},
        {.ink{static_cast<std::uint8_t>(level + 229)}, .edge{static_cast<std::uint8_t>(level == 1 ? 0 : level + 223)}});
    }
  }
}

void draw_missile_camera_indicator(framework::render::indexed_cockpit_framebuffer &target,
  std::uint16_t const clock, bool const enabled, bool const following) {
  /// 54A3: missile views stay lit; other views blink using timer bits 7 and 8
  if(!following && (!enabled || !(clock & 0x180))) return;
  for(int y{0}; y < 3; ++y) {
    for(int x{300}; x < 304; ++x) put_pixel(target, x, y, 0xbe);
  }
}

attitude_line calculate_attitude(std::uint16_t const pitch_index, std::uint16_t const roll_index,
  std::int8_t const pitch_high, bool const alternate_colour, int const centre_y) {
  /// 5E6F scales signed table words with high-word products before constructing the endpoints
  if(pitch_index >= 1024 || roll_index >= 1024) throw std::invalid_argument{"attitude table index must be below 1024"};
  int const pitch_sine{(maths::original_sine[pitch_index] * 224) >> 16};
  int const pitch_cosine{(maths::original_sine[(pitch_index + 256) % 1024] * 224) >> 16};
  int const roll_sine{maths::original_sine[roll_index]};
  int const roll_cosine{maths::original_sine[(roll_index + 256) % 1024]};
  pixel_position const centre{160 + ((roll_sine * pitch_sine) >> 16), centre_y + ((roll_cosine * pitch_sine) >> 16)};
  pixel_position const offset{(roll_cosine * pitch_cosine) >> 16, -((roll_sine * pitch_cosine) >> 16)};
  int const shade{(pitch_high * (pitch_high < 0 ? -45 : 44)) >> 8};
  return {
    .first{centre - offset},
    .last{centre + offset},
    .colour{static_cast<std::uint8_t>((alternate_colour ? 238 : 14) + shade)},
  };
}

void draw_target_marker(framework::render::indexed_cockpit_framebuffer &target,
  target_marker const marker, pixel_position const &centre, std::uint8_t const upper_colour, std::uint8_t const lower_colour) {
  /// 5F39 emits paired outlines; E302/E305 consume horizontal/vertical step bits
  std::span<std::uint8_t const> stream;
  switch(marker) {
  case target_marker::small: stream = marker_small; break;
  case target_marker::large: stream = marker_large; break;
  case target_marker::skimma_aim: stream = marker_skimma; break;
  default: throw std::invalid_argument{"unknown target marker"};
  }
  if(centre.x < 0 || centre.x >= 320 || centre.y < 0 || centre.y >= 240) throw std::invalid_argument{"marker centre must be inside the display"};
  int const extent{stream.front()};
  std::size_t cursor{1};
  for(int half{0}; half < 2; ++half) {
    int const y{centre.y + (half ? extent - 1 : -extent)};
    int const step{half ? -1 : 1};
    cursor += draw_outline(target, stream.subspan(cursor), {centre.x - 1, y}, {centre.x, y},
      step, step, half ? lower_colour : upper_colour);
  }
}

void draw_attitude_surround(framework::render::indexed_cockpit_framebuffer &target, std::uint16_t const colour_parameter, int const centre_y) {
  /// 5ED6 draws two fixed outlines, then a pair converging vertically around the attitude centre
  std::span<std::uint8_t const> stream{attitude_outline};
  auto consumed{draw_outline(target, stream, {159, centre_y + 57}, {160, centre_y + 57}, -1, -1, 19)};
  stream = stream.subspan(consumed);
  consumed = draw_outline(target, stream, {136, centre_y - 53}, {184, centre_y - 53}, 1, 1, 19);
  stream = stream.subspan(consumed);
  auto const colour{static_cast<std::uint8_t>(234 + ((colour_parameter & 255) & (colour_parameter >> 8)))};
  draw_outline(target, stream, {160, centre_y + 11}, {160, centre_y - 11}, -1, 1, colour);
}

} // namespace darker::graphics
