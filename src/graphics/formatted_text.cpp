#include "graphics/formatted_text.h"
#include <array>
#include <bit>
#include <stdexcept>

namespace darker::graphics {
namespace {

class formatter {
private:
  std::span<std::byte const> text;
  resources::font_resource const &font;
  resources::font_face face;
  formatted_page &page;

  uint8_t byte() {
    /// Never let a malformed page consume the next record or an unrelated allocation
    if(page.consumed == text.size()) throw std::invalid_argument{"Unterminated or truncated formatted text"};
    return std::to_integer<uint8_t>(text[page.consumed++]);
  }

  void glyph(uint8_t const code) {
    /// Record original coordinates and colour tokens; palette translation remains the drawing consumer's job
    auto &cursor{page.cursor};
    page.glyphs.push_back({
      .position{std::bit_cast<int16_t>(cursor.x), std::bit_cast<int16_t>(cursor.y)},
      .colour{cursor.colour},
      .code{code}
    });
    cursor.x = static_cast<uint16_t>(cursor.x + font.glyph(face, code).advance);
  }

  uint16_t centred_width() const {
    /// E2C1 counts stored widths rather than spacing-adjusted advances, stopping only at zero
    uint16_t width{0};
    for(auto const value : text.subspan(page.consumed)) {
      auto const code{std::to_integer<uint8_t>(value)};
      if(!code) return width;
      width = static_cast<uint16_t>(width + (code < 33 ? 4 : font.glyph(face, code).width));
    }
    throw std::invalid_argument{"Unterminated centred text"};
  }

public:
  formatter(std::span<std::byte const> const source, resources::font_resource const &resource,
    resources::font_face const selection, formatted_page &output) : text{source}, font{resource}, face{selection}, page{output} {
    /// Share one state and byte cursor across nested runs, as B292/B294 do
  }

  void run(unsigned int const depth = 0) {
    /// Translate B292's control dispatch while preserving word arithmetic and persistent margin/colour state
    if(depth > 32) throw std::invalid_argument{"Formatted text nesting exceeds the supported depth"};
    auto &cursor{page.cursor};
    for(;;) {
      auto const code{byte()};
      if(code >= 32) {
        glyph(code);
        continue;
      }
      switch(code) {
      case 0:
        return;
      case 1:
        cursor.y = byte();
        cursor.x = byte();
        break;
      case 2:
        {
          auto const low{byte()};
          auto const colour{static_cast<uint16_t>(low | byte() << 8)};
          if(colour != 0xffff) cursor.colour = colour;
        }
        break;
      case 3:
        cursor.x = cursor.margin;
        cursor.y = static_cast<uint16_t>(cursor.y + cursor.line_step);
        break;
      case 4:
        cursor.x = static_cast<uint16_t>(((static_cast<uint16_t>(cursor.x - cursor.margin)) | 15) + 1 + cursor.margin);
        break;
      case 5:
        cursor.margin = byte();
        cursor.line_step = std::bit_cast<int8_t>(byte());
        cursor.x = cursor.margin;
        break;
      case 6:
        cursor.x = static_cast<uint16_t>(320 - centred_width()) >> 1;
        run(depth + 1);
        break;
      case 7:
        {
          std::array<uint8_t, 3> digits{};
          size_t count{0};
          auto value{cursor.runtime_number};
          do {
            digits[count++] = static_cast<uint8_t>('0' + value % 10);
            value /= 10;
          } while(value);
          while(count) glyph(digits[--count]);
        }
        break;
      default:
        throw std::invalid_argument{"Unknown original text formatting command"};
      }
    }
  }
};

} // anonymous namespace

formatted_page lay_out_text(std::span<std::byte const> const text, resources::font_resource const &font,
  resources::font_face const face, text_cursor const cursor) {
  /// Consume exactly one terminated page, leaving the next page or counted-message suffix to its caller
  formatted_page page{
    .cursor{cursor}
  };
  formatter{text, font, face, page}.run();
  return page;
}

} // namespace darker::graphics
