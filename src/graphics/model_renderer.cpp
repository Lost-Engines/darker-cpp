#include "graphics/model_renderer.h"
#include <algorithm>
#include <bit>
#include <format>
#include <stdexcept>
#include "graphics/screen_primitives.h"
#include "maths/sine_table.h"

namespace darker::graphics {
namespace {

std::int32_t signed_coordinate(std::int32_t const value) noexcept {
  /// Keep three-byte interpolation additions and subtractions within their original signed range
  return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value) << 8) >> 8;
}

std::int16_t signed_word(int const value) noexcept {
  /// Interpolation quantises deltas to signed words before multiplying
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

class interpreter {
private:
  framework::render::indexed_cockpit_framebuffer &target;
  std::span<std::byte const> pool;
  model_projection projection;
  model_colours const &colours;
  model_animation const &animation;
  camera_vertex anchor{};
  std::uint16_t interpolation{0};
  std::int16_t last_depth{0};
  std::uint8_t distance_high;
  int bottom;
  model_path path;
  model_shading shading;
  screen_vertex screen_origin;
  std::array<camera_vertex, 256> camera_vertices{};
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
    camera_vertices[cursor] = projection.transform();
    last_depth = signed_word(camera_vertices[cursor].depth >> 8);
    if(path == model_path::direct) vertices[cursor] = project_vertex(camera_vertices[cursor], screen_origin);
    defined[cursor++] = true;
  }

  std::int16_t parameter(std::size_t &position) const {
    /// 319E adds the low five cell-state bits to the operand with byte wrapping
    return animation.parameters[static_cast<std::uint8_t>((animation.cell_state & 31) + byte(position))];
  }

  void save_anchor() {
    /// The direct path saves live accumulators; the near path retains the last emitted camera vertex
    if(path == model_path::near_clipped) {
      if(cursor == 0) throw std::invalid_argument{"Model anchor has no preceding vertex"};
      anchor = camera_vertices[cursor - 1];
    } else {
      anchor = projection.transform();
      anchor.vertical = signed_coordinate(anchor.vertical + 0xba);             // 31D9 includes the adjacent MOV opcode byte at FCD4
      anchor.depth = last_depth * 256;
    }
  }

  void emit_interpolated() {
    /// 3215/32AE use different quantised interpolation paths before and after camera-space vertex storage
    if(cursor == 0 || cursor >= vertices.size()) throw std::invalid_argument{"Interpolated model vertex has no valid output slot"};
    auto current{path == model_path::near_clipped ? camera_vertices[cursor - 1] : projection.transform()};
    if(path == model_path::direct) current.vertical = signed_coordinate(current.vertical + 0xba);
    camera_vertex result{};
    if(path == model_path::near_clipped) {
      int const factor{interpolation >> 4};
      result.horizontal = signed_coordinate(anchor.horizontal + ((signed_word(signed_coordinate(current.horizontal - anchor.horizontal) >> 10) * factor) >> 2));
      result.vertical = signed_coordinate(anchor.vertical + ((signed_word(signed_coordinate(current.vertical - anchor.vertical) >> 10) * factor) >> 2));
      result.depth = signed_coordinate(anchor.depth + ((signed_word(signed_coordinate(current.depth - anchor.depth) >> 2) * factor) >> 10));
      last_depth = signed_word(result.depth >> 8);
    } else {
      int const factor{interpolation >> 1};
      result.horizontal = signed_coordinate(anchor.horizontal + ((signed_word(signed_coordinate(current.horizontal - anchor.horizontal) >> 7) * factor) >> 8));
      result.vertical = signed_coordinate(anchor.vertical + ((signed_word(signed_coordinate(current.vertical - anchor.vertical) >> 7) * factor) >> 8));
      result.depth = signed_word((anchor.depth >> 8) + ((signed_word((last_depth - (anchor.depth >> 8)) * 2) * factor) >> 16)) * 256;
      vertices[cursor] = project_vertex(result, screen_origin);
    }
    camera_vertices[cursor] = result;
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
    projection_parameters const parameters, model_colours const &palette, int const height, model_path const drawing_path, model_animation const &animation_state, model_shading const shading_mode)
    : target{frame}, pool{bytes}, projection{parameters}, colours{palette}, animation{animation_state}, distance_high{static_cast<std::uint8_t>(parameters.depth.whole >> 8)}, bottom{height}, path{drawing_path}, shading{shading_mode}, screen_origin{parameters.origin} {
    /// Keep bytecode, projection cache and vertex storage together for one drawing invocation
  }

  void run(std::size_t position, unsigned int const depth = 0) {
    /// Follow the original drawing commands with either direct or near-clipped vertex storage
    if(depth > 40) throw std::invalid_argument{"Model call nesting exceeds the supported bound"};
    while(true) {
      if(++instructions > 10000) throw std::invalid_argument{"Model instruction limit exceeded"};
      auto const at{position};
      auto const opcode{byte(position)};
      switch(opcode) {
      case 0x0d: return;
      case 0x32: interpolation = static_cast<std::uint16_t>(parameter(position)); save_anchor(); break;
      case 0x35: save_anchor(); break;
      case 0x38: emit_interpolated(); break;
      case 0x3b: projection.set_component(2, parameter(position)); break;
      case 0x22:
        {
          auto const index{colour(byte(position))};
          auto const radius{word(position)};
          if(cursor == 0) throw std::invalid_argument{"Model disc has no preceding vertex"};
          --cursor;
          auto const centre{camera_vertices[cursor]};
          auto const depth{path == model_path::near_clipped ? signed_word(centre.depth >> 8) : last_depth};
          if(path == model_path::near_clipped && depth < 32) break;
          if(depth == 0) throw std::domain_error{"Model disc has zero depth"};
          auto const point{path == model_path::near_clipped ? project_vertex(centre, screen_origin) : vertices[cursor]};
          draw_disc(target, {.x{point.x}, .y{point.y}}, radius / static_cast<std::uint16_t>(depth), index, bottom);
        }
        break;
      case 0x1e:
        {
          auto const index{colour(byte(position))};
          auto const a{byte(position)};
          auto const b{byte(position)};
          if(!defined[a] || !defined[b]) throw std::invalid_argument{"Model line references an undefined vertex"};
          screen_vertex first{}, last{};
          if(path == model_path::near_clipped) {
            auto const left{camera_vertices[a]}, right{camera_vertices[b]};
            bool const left_inside{left.depth >= 32 * 256}, right_inside{right.depth >= 32 * 256};
            if(!left_inside && !right_inside) break;
            first = left_inside ? project_vertex(left, screen_origin) : near_intersection(right, left, screen_origin);
            last = right_inside ? project_vertex(right, screen_origin) : near_intersection(left, right, screen_origin);
          } else {
            first = vertices[a]; last = vertices[b];
          }
          draw_world_line(target, {.x{first.x}, .y{first.y}}, {.x{last.x}, .y{last.y}}, index, bottom);
        }
        break;
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
          auto const source_colour{byte(position)};
          auto const index{colour(source_colour)};
          std::array<screen_vertex, 260> face{};
          std::array<shaded_vertex, 260> shaded_face{};
          std::array<std::uint16_t, 256> vertex_shades{};
          std::array<camera_vertex, 256> camera_face{};
          for(unsigned int i{0}; i < count; ++i) {
            auto const source{byte(position)};
            if(!defined[source]) throw std::invalid_argument{"Model face references an undefined vertex"};
            if(path == model_path::near_clipped) camera_face[i] = camera_vertices[source];
            else face[i] = vertices[source];
            if(shaded) {
              auto const operand{byte(position)};
              if(shading == model_shading::gouraud) {
                if(operand >= colours.shades.size()) throw std::invalid_argument{"Vertex shade exceeds the original palette ramp"};
                auto const shade{colours.shades[operand]};
                vertex_shades[i] = static_cast<std::uint16_t>(((source_colour & 224) + shade) * 256 + shade + 128);
                shaded_face[i] = {.x{face[i].x}, .y{face[i].y}, .shade{vertex_shades[i]}};
              }
            }
          }
          if(shaded && shading == model_shading::gouraud) {
            auto const projected_count{path == model_path::near_clipped
              ? clip_near_shaded_polygon(std::span{camera_face}.first(count), std::span{vertex_shades}.first(count), screen_origin, shaded_face) : count};
            draw_gouraud_polygon(target, std::span{shaded_face}.first(projected_count), 319, bottom);
            break;
          }
          auto const projected_count{path == model_path::near_clipped
            ? clip_near_polygon(std::span{camera_face}.first(count), screen_origin, face) : count};
          draw_flat_polygon(target, std::span{face}.first(projected_count), index, 319, bottom);
        }
        break;
      case 0x16:
        {
          std::array<std::uint8_t, 3> const indices{byte(position), byte(position), byte(position)};
          for(auto const index : indices) {
            if(!defined[index]) throw std::invalid_argument{"Model visibility test references an undefined vertex"};
          }
          auto const skip{byte(position)};
          bool hidden{false};
          if(path == model_path::near_clipped) {
            std::array const triangle{camera_vertices[indices[0]], camera_vertices[indices[1]], camera_vertices[indices[2]]};
            std::array<screen_vertex, 6> clipped{};
            auto const count{clip_near_polygon(triangle, screen_origin, clipped)};
            hidden = count == 0 || back_facing(clipped[0], clipped[1], clipped[2]);
          } else {
            hidden = back_facing(vertex(indices[0]), vertex(indices[1]), vertex(indices[2]));
          }
          if(hidden) position += skip;
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

void update_fountain_parameters(model_animation &animation, std::uint16_t const clock) noexcept {
  /// DB1A resets six fountain heights and offsets the two active bands using the original sine table
  std::fill_n(animation.parameters.begin() + 9, 6, static_cast<std::int16_t>(-384));
  std::array<int, 7> constexpr amplitudes{118, 154, 136, 112, 85, 60, 118};          // original bytes at DB6A
  auto const phase{static_cast<std::uint32_t>(static_cast<std::uint16_t>(clock << 5)) * 6};
  auto const band{phase >> 16};
  auto const sine{maths::original_sine[(phase & 65535) >> 7]};
  animation.parameters[9 + band] += static_cast<std::int16_t>((amplitudes[band] * sine) >> 16);
  animation.parameters[9 + (band + 1) % 6] -= static_cast<std::int16_t>((2 * amplitudes[band + 1] * sine) >> 16);
}

void draw_model(framework::render::indexed_cockpit_framebuffer &target, std::span<std::byte const> const pool,
  std::size_t const model_offset, projection_parameters const projection, model_colours const &colours, int const bottom, model_path const path, model_animation const &animation, model_shading const shading) {
  /// The common eleven-byte model header precedes drawing code for both city and special definitions
  if(model_offset > pool.size() || pool.size() - model_offset < 12) throw std::invalid_argument{"Model has no complete header and drawing body"};
  if(bottom <= 0 || bottom > 240) throw std::invalid_argument{"Model viewport exceeds the framebuffer height"};
  interpreter{target, pool, projection, colours, bottom, path, animation, shading}.run(model_offset + 11);
}

} // namespace darker::graphics
