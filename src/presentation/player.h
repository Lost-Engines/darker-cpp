#pragma once

#include <array>
#include <optional>
#include "graphics/font.h"
#include "graphics/formatted_text.h"
#include "graphics/palette_bitmap.h"
#include "resources/archive_set.h"
#include "resources/scenario.h"

namespace darker::presentation {

struct landing_entry { uint16_t site; uint8_t heading; };

struct animation_pixel { uint16_t x; uint16_t y; uint8_t colour; };
using animation_frame = std::vector<animation_pixel>;
std::vector<animation_frame> decode_animation(std::span<std::byte const> data);

class player {
private:
  resources::archive_set const &archives;
  resources::font_resource const &font;
  std::span<std::byte const> program;
  std::span<std::byte const> text;
  size_t cursor{0}, checkpoint{0};
  std::optional<size_t> next;
  size_t text_cursor{0};
  uint32_t ticks{0}, pending{0}, deadline{0};
  uint8_t interval{1}, selected{7}, object_counter{0};
  bool stopped{false}, repeat_delay{false};
  struct animation_pair { uint8_t current{160}; uint8_t target{160}; };
  std::array<animation_pair, 12> pairs{};
  std::array<std::vector<animation_frame>, 2> animations;
  std::array<size_t,2> animation_counts{};
  framework::render::indexed_cockpit_framebuffer background{};
  graphics::palette_state colours;
  std::vector<std::byte> image_pixels;
  graphics::formatted_page page;
  struct caption { std::span<std::byte const> text; uint32_t expiry{0}; int16_t x{0}; uint16_t width{0}; };
  std::array<caption,3> captions{};
  uint8_t caption_y{229}, caption_width_extension{0};
  uint16_t caption_colours{0xfffe};
  resources::font_face face{resources::font_face::compact};
  uint8_t text_y{0}, image_height{0}, image_y{0};
  uint16_t image_width{0}, image_x{0};
  uint8_t byte();
  uint16_t word();
  void execute();
  void load_image(resources::resource_id id);
  void draw_image(unsigned int width, unsigned int height, unsigned int x, unsigned int y);
  void image(resources::resource_id id, unsigned int width, unsigned int height, unsigned int x, unsigned int y);

public:
  uint16_t weapon_toggles{0};
  uint16_t departure_destination{0};
  std::optional<landing_entry> entry;
  std::optional<uint8_t> music;
  uint8_t input_policy{5};
  player(resources::archive_set const &archives, resources::font_resource const &font,
    resources::scenario_resource const &scenario, size_t record, uint8_t completed_objects = 0);
  void advance(uint32_t elapsed_ticks);
  bool continue_page();
  bool finished() const noexcept;
  size_t consumed_text() const noexcept;
  void draw(framework::render::cockpit_framebuffer &output) const;
};

} // namespace darker::presentation
