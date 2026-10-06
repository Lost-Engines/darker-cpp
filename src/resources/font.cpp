#include "resources/font.h"
#include <array>
#include <stdexcept>
#include <utility>

namespace darker::resources {
namespace {

struct font_directory {
  std::size_t offset;
  unsigned int count;
  unsigned int spacing;
};

std::array<font_directory, 3> constexpr directories{{{0, 107, 0}, {0x226a, 97, 1}, {0x3758, 96, 1}}};

} // namespace

font_resource::font_resource(std::vector<std::byte> resource) : data{std::move(resource)} {
  /// Validate the three original E29C directories and all four VGA alignment paths before drawing
  for(std::size_t face{0}; face < directories.size(); ++face) {
    for(unsigned int code{33}; code < 33 + directories[face].count; ++code) {
      for(unsigned int phase{0}; phase < 4; ++phase) glyph(static_cast<font_face>(face), static_cast<std::uint8_t>(code), phase);
    }
  }
}

font_glyph font_resource::glyph(font_face const face, std::uint8_t const code, unsigned int const alignment) const {
  /// E1FE uses a phase-relative offset table; low/high nibbles select coverage and the two-colour pattern
  auto const &font{directories.at(static_cast<std::size_t>(face))};
  if(alignment > 3) throw std::out_of_range{"Font alignment must be between zero and three"};
  if(code < 33) return {.advance{4}};
  unsigned int const index{code - 33u};
  if(index >= font.count) throw std::out_of_range{"Glyph code is outside the original font"};
  auto const byte{[&](std::size_t const offset){
    if(offset >= data.size()) throw std::invalid_argument{"Truncated original font resource"};
    return std::to_integer<std::uint8_t>(data[offset]);
  }};
  auto const width{byte(font.offset + font.count + index * 2)};
  auto const height{byte(font.offset + font.count + index * 2 + 1)};
  auto const top{byte(font.offset + index)};
  auto const table{font.offset + font.count * (3 + alignment * 2)};
  auto const source{table + byte(table + index * 2) + (byte(table + index * 2 + 1) << 8)};
  auto const stride{(width + alignment + 3) / 4};
  auto const size{stride * height};
  if(source > data.size() || size > data.size() - source) throw std::invalid_argument{"Font glyph exceeds its resource"};
  return {
    .width{width}, .height{height}, .top{top}, .advance{static_cast<std::uint16_t>(width + font.spacing)},
    .stride{stride}, .planes{std::span{data}.subspan(source, size)},
  };
}

} // namespace darker::resources
