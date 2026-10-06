#include "graphics/model_renderer.h"
#include <algorithm>
#include <bit>
#include <format>
#include <stdexcept>

namespace darker::graphics {
namespace {

class interpreter {
private:
  framework::render::indexed_cockpit_framebuffer &target;
  std::span<std::byte const> pool;
  model_projection projection;
  model_colours const &colours;
  std::uint8_t distance_high;
  int bottom;
  std::array<screen_vertex, 256> vertices{};
  std::array<bool, 256> defined{};
  std::size_t cursor{0};
  unsigned int instructions{0};

  std::uint8_t byte(std::size_t &position) const {
    /// Consume a checked byte from the original geometry pool
    if(position >= pool.size()) throw std::invalid_argument{"Model instruction exceeds its geometry pool"};
    return std::to_integer<std::uint8_t>(pool[position++]);
  }

  std::uint16_t word(std::size_t &position) const {
    /// Consume an unaligned little-endian operand
    auto const low{byte(position)};
    return static_cast<std::uint16_t>(low | byte(position) << 8);
  }

  std::size_t relative(std::size_t const operand, std::uint16_t const displacement) const {
    /// Relative calls and jumps are based at the displacement word itself
    auto const destination{static_cast<std::ptrdiff_t>(operand) + std::bit_cast<std::int16_t>(displacement)};
    if(destination < 0 || static_cast<std::size_t>(destination) >= pool.size()) throw std::invalid_argument{"Model branch exceeds its geometry pool"};
    return static_cast<std::size_t>(destination);
  }

  screen_vertex vertex(std::uint8_t const index) const {
    /// Reject references to vertices not emitted by this model invocation
    if(!defined[index]) throw std::invalid_argument{"Model face references an undefined vertex"};
    return vertices[index];
  }

  void emit() {
    /// Store a projected vertex at the current original output index
    if(cursor >= vertices.size()) throw std::invalid_argument{"Model exceeds its projected vertex buffer"};
    vertices[cursor] = projection.project().screen;
    defined[cursor++] = true;
  }

  void set(std::size_t const axis, std::size_t &position) {
    /// Replace one cached coordinate contribution from its signed operand
    projection.set_component(axis, std::bit_cast<std::int16_t>(word(position)));
  }

  std::uint8_t colour(std::uint8_t const source) const noexcept {
    /// 3383 resolves the supplied distance shade table and dynamic colour codes
    unsigned int shade{static_cast<unsigned int>(source & 31)};
    if(shade < 28) shade = colours.shades[shade];
    else {
      unsigned int const value{static_cast<std::uint8_t>((colours.dynamic & 31) + colours.shades[27])};
      shade = std::min(27u, static_cast<unsigned int>(static_cast<std::uint8_t>(value + 11 - (value >> 2))) >> 1);
    }
    return static_cast<std::uint8_t>((source & 224) + shade);
  }

public:
  interpreter(framework::render::indexed_cockpit_framebuffer &frame, std::span<std::byte const> const bytes,
    projection_parameters const parameters, model_colours const &palette, int const height)
    : target{frame}, pool{bytes}, projection{parameters}, colours{palette}, distance_high{static_cast<std::uint8_t>(parameters.depth.whole >> 8)}, bottom{height} {
    /// Keep bytecode, projection cache and vertex storage together for one drawing invocation
  }

  void run(std::size_t position, unsigned int const depth = 0) {
    /// Interpret the far-path flat model commands without flattening calls or visibility branches
    if(depth > 40) throw std::invalid_argument{"Model call nesting exceeds the supported bound"};
    while(true) {
      if(++instructions > 10000) throw std::invalid_argument{"Model instruction limit exceeded"};
      auto const at{position};
      auto const opcode{byte(position)};
      switch(opcode) {
      case 0x0d: return;
      case 0x3e:
      case 0x4d:
        {
          auto const operand{position};
          auto const destination{relative(operand, word(position))};
          if(opcode == 0x3e) run(destination, depth + 1);
          else position = destination;
        }
        break;
      case 0x2c: cursor = 0; break;
      case 0x25:
        {
          auto const address{word(position)};
          if(address < 0xfc00 || (address & 3)) throw std::invalid_argument{"Invalid projected vertex cursor"};
          cursor = (address - 0xfc00) / 4;
        }
        break;
      case 0x52: projection.zero_component(2); break;
      case 0x56: projection.zero_component(0); break;
      case 0x5a: projection.zero_component(1); break;
      case 0x67: projection.negate_component(2); break;
      case 0x6b: projection.negate_component(0); break;
      case 0x6f: projection.negate_component(1); break;
      case 0x5e: projection.zero_component(2); emit(); break;
      case 0x61: projection.zero_component(0); emit(); break;
      case 0x64: projection.zero_component(1); emit(); break;
      case 0x73: projection.negate_component(2); emit(); break;
      case 0x76: projection.negate_component(0); emit(); break;
      case 0x79: projection.negate_component(1); emit(); break;
      case 0x7c: set(1, position); set(2, position); emit(); break;
      case 0x7f: set(2, position); emit(); break;
      case 0x85: set(2, position); set(0, position); emit(); break;
      case 0x88: set(0, position); emit(); break;
      case 0x8e: set(2, position); set(0, position); set(1, position); emit(); break;
      case 0x91: set(0, position); set(1, position); emit(); break;
      case 0x94: set(1, position); emit(); break;
      case 0x97: emit(); break;
      case 0x01:
      case 0x02:
      case 0x03:
      case 0x0e:
      case 0x0f:
      case 0x11:
        {
          bool const shaded{opcode >= 0x0e};
          unsigned int const count{static_cast<unsigned int>(opcode == 1 || opcode == 0x11 ? byte(position) : shaded ? opcode - 12 : opcode) + 1};
          auto const index{colour(byte(position))};
          std::array<screen_vertex, 256> face{};
          for(unsigned int i{0}; i < count; ++i) {
            face[i] = vertex(byte(position));
            if(shaded) byte(position);                                         // 312A's flat mode skips each Gouraud shade operand
          }
          draw_flat_polygon(target, std::span{face}.first(count), index, 319, bottom);
        }
        break;
      case 0x16:
        {
          auto const a{vertex(byte(position))};
          auto const b{vertex(byte(position))};
          auto const c{vertex(byte(position))};
          auto const skip{byte(position)};
          if(back_facing(a, b, c)) position += skip;
        }
        break;
      case 0x1a:
        {
          auto const threshold{byte(position)};
          auto const skip{byte(position)};
          if(static_cast<std::uint8_t>(distance_high - threshold) & 128) position += skip;
        }
        break;
      default: throw std::invalid_argument{std::format("Unsupported flat model opcode {:02x} at pool offset {:04x}", opcode, at)};
      }
    }
  }
};

} // namespace

void draw_flat_model(framework::render::indexed_cockpit_framebuffer &target, std::span<std::byte const> const pool,
  std::size_t const model_offset, projection_parameters const projection, model_colours const &colours, int const bottom) {
  /// The common eleven-byte model header precedes drawing code for both city and special definitions
  if(model_offset > pool.size() || pool.size() - model_offset < 12) throw std::invalid_argument{"Model has no complete header and drawing body"};
  if(bottom <= 0 || bottom > 240) throw std::invalid_argument{"Model viewport exceeds the framebuffer height"};
  interpreter{target, pool, projection, colours, bottom}.run(model_offset + 11);
}

} // namespace darker::graphics
