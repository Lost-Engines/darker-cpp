#include "graphics/palette_bitmap.h"
#include <cstdint>
#include <stdexcept>
#include "maths/sine_table.h"

namespace darker::graphics {

uint8_t palette_fade_gain(uint16_t const phase) noexcept {
  /// AFA2 rounds the sine table's high byte into the original 0..64 brightness coefficient
  auto const high{static_cast<uint8_t>(static_cast<uint16_t>(maths::original_sine[phase & 511]) >> 8)};
  return static_cast<uint8_t>((high + 1) >> 1);
}

uint8_t palette_dac_component(uint8_t const component, uint8_t const gain) noexcept {
  /// AFAD builds a 256-entry component lookup using byte carry accumulation before the six-bit VGA DAC
  return static_cast<uint8_t>((component * gain + (gain >> 1)) >> 8);
}

framework::render::colour_palette fade_palette(framework::render::colour_palette const &colours, uint16_t const phase) noexcept {
  /// Expand the native DAC values back to framebuffer RGB; the original palette remains unchanged
  auto result{colours};
  auto const gain{palette_fade_gain(phase)};
  for(auto &colour : result) {
    colour.r = static_cast<uint8_t>(palette_dac_component(colour.r, gain) * 4);
    colour.g = static_cast<uint8_t>(palette_dac_component(colour.g, gain) * 4);
    colour.b = static_cast<uint8_t>(palette_dac_component(colour.b, gain) * 4);
  }
  return result;
}

palette_update decode_palette(std::span<std::byte const> const data, palette_state previous) {
  /// Reproduce the palette skip/literal stream used by original routines 4185/4190
  size_t position{0};
  size_t index{0};
  while(index < previous.colours.size()) {
    if(position == data.size()) throw std::runtime_error{"truncated palette command"};
    auto const value{std::to_integer<uint8_t>(data[position++])};
    if(value & 1) {
      index += (value >> 1) + 1;
      if(index > previous.colours.size()) throw std::runtime_error{"palette skip exceeds 256 entries"};
    } else {
      if(data.size() - position < 2) throw std::runtime_error{"truncated palette colour"};
      previous.colours[index] = {
        value,
        std::to_integer<uint8_t>(data[position]),
        std::to_integer<uint8_t>(data[position + 1]),
      };
      previous.defined.set(index++);
      position += 2;
    }
  }
  return {
    .palette{previous},
    .bytes_consumed{position}
  };
}

palette_bitmap decode_bitmap(std::span<std::byte const> const data, palette_state previous) {
  /// Decode only the known 320 by 200 sheets, rejecting undefined colours and other layouts
  auto const update{decode_palette(data, previous)};
  auto const pixels{data.subspan(update.bytes_consumed)};
  palette_bitmap result{
    .palette{update.palette},
    .image{}
  };
  if(pixels.size() != result.image.pixels.size()) throw std::runtime_error{"expected a 320x200 source bitmap"};
  for(size_t i{0}; i < pixels.size(); ++i) {
    auto const index{std::to_integer<uint8_t>(pixels[i])};
    if(!result.palette.defined[index]) throw std::runtime_error{"bitmap uses an undefined palette entry"};
    result.image.pixels[i] = index;
  }
  return result;
}

} // namespace darker::graphics
