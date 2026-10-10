#include "graphics/model_renderer.h"
#include <algorithm>
#include <bit>
#include <format>
#include <stdexcept>
#include <utility>
#include "graphics/screen_layout.h"
#include "graphics/screen_primitives.h"
#include "maths/sine_table.h"

namespace darker::graphics {
namespace {

enum class model_opcode : uint8_t {
  flat_polygon = 0x01,
  flat_triangle = 0x02,
  flat_quad = 0x03,
  return_from_model = 0x0d,
  shaded_triangle = 0x0e,
  shaded_quad = 0x0f,
  shaded_polygon = 0x11,
  skip_back_facing = 0x16,
  skip_near = 0x1a,
  line = 0x1e,
  disc = 0x22,
  set_vertex_cursor = 0x25,
  reset_vertex_cursor = 0x2c,
  set_interpolation_and_anchor = 0x32,
  save_anchor = 0x35,
  emit_interpolated = 0x38,
  set_animated_c = 0x3b,
  call = 0x3e,
  jump = 0x4d,
  zero_c = 0x52,
  zero_a = 0x56,
  zero_b = 0x5a,
  negate_c = 0x67,
  negate_a = 0x6b,
  negate_b = 0x6f,
  zero_c_and_emit = 0x5e,
  zero_a_and_emit = 0x61,
  zero_b_and_emit = 0x64,
  negate_c_and_emit = 0x73,
  negate_a_and_emit = 0x76,
  negate_b_and_emit = 0x79,
  set_bc_and_emit = 0x7c,
  set_c_and_emit = 0x7f,
  set_ca_and_emit = 0x85,
  set_a_and_emit = 0x88,
  set_cab_and_emit = 0x8e,
  set_ab_and_emit = 0x91,
  set_b_and_emit = 0x94,
  emit = 0x97,
};


render_geometry::accumulator signed_coordinate(render_geometry::accumulator const value) noexcept {
  /// Keep three-byte interpolation additions and subtractions within their original signed range
  return render_geometry::wrap_projection(static_cast<render_geometry::accumulator_bits>(value));
}

render_geometry::coordinate signed_word(render_geometry::accumulator const value) noexcept {
  /// Interpolation quantises deltas to signed words before multiplying
  return render_geometry::wrap_coordinate(value);
}

class interpreter {
private:
  framework::render::indexed_surface target;
  std::span<std::byte const> pool;
  model_projection projection;
  model_colours const &colours;
  model_animation const &animation;
  camera_vertex anchor{};
  uint16_t interpolation{0};
  render_geometry::coordinate last_depth{0};
  uint8_t distance_high;
  raster_viewport viewport;
  model_path path;
  model_shading shading;
  screen_vertex screen_origin;
  std::array<camera_vertex, 256> camera_vertices{};
  std::array<screen_vertex, 256> vertices{};
  std::array<bool, 256> defined{};
  size_t cursor{0};
  unsigned int instructions{0};

  uint8_t byte(size_t &position) const {
    /// Consume a checked byte from the original geometry pool
    if(position >= pool.size()) throw std::invalid_argument{"Model instruction exceeds its geometry pool"};
    return std::to_integer<uint8_t>(pool[position++]);
  }

  uint16_t word(size_t &position) const {
    /// Consume an unaligned little-endian operand
    auto const low{byte(position)};
    return static_cast<uint16_t>(low | byte(position) << 8);
  }

  size_t relative(size_t const operand, uint16_t const displacement) const {
    /// Relative calls and jumps are based at the displacement word itself
    auto const destination{static_cast<ptrdiff_t>(operand) + std::bit_cast<int16_t>(displacement)};
    if(destination < 0 || static_cast<size_t>(destination) >= pool.size()) throw std::invalid_argument{"Model branch exceeds its geometry pool"};
    return static_cast<size_t>(destination);
  }

  screen_vertex const &vertex(uint8_t const index) const {
    /// Reject references to vertices not emitted by this model invocation
    if(!defined[index]) throw std::invalid_argument{"Model face references an undefined vertex"};
    return vertices[index];
  }

  void emit() {
    /// Store a projected vertex at the current original output index
    if(cursor >= vertices.size()) throw std::invalid_argument{"Model exceeds its projected vertex buffer"};
    camera_vertices[cursor] = projection.transform();
    last_depth = signed_word(camera_vertices[cursor].depth >> render_geometry::fraction_bits);
    if(path == model_path::direct) vertices[cursor] = project_vertex(camera_vertices[cursor], screen_origin);
    defined[cursor++] = true;
  }

  int16_t parameter(size_t &position) const {
    /// 319E adds the low five cell-state bits to the operand with byte wrapping
    return animation.parameters[static_cast<uint8_t>((animation.cell_state & 31) + byte(position))];
  }

  void save_anchor() {
    /// The direct path saves live accumulators; the near path retains the last emitted camera vertex
    if(path == model_path::near_clipped) {
      if(cursor == 0) throw std::invalid_argument{"Model anchor has no preceding vertex"};
      anchor = camera_vertices[cursor - 1];
    } else {
      anchor = projection.transform();
      anchor.vertical = signed_coordinate(anchor.vertical + 0xba);             // 31D9 includes the adjacent MOV opcode byte at FCD4
      anchor.depth = last_depth * render_geometry::fraction_scale;
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
      // 32DC/330E read each endpoint from byte one: truncate both before subtraction
      result.horizontal = signed_coordinate(anchor.horizontal + ((signed_word(((current.horizontal >> render_geometry::fraction_bits) - (anchor.horizontal >> render_geometry::fraction_bits)) >> 2) * factor) >> 2));
      result.vertical = signed_coordinate(anchor.vertical + ((signed_word(((current.vertical >> render_geometry::fraction_bits) - (anchor.vertical >> render_geometry::fraction_bits)) >> 2) * factor) >> 2));
      result.depth = signed_coordinate(anchor.depth + ((signed_word(signed_coordinate(current.depth - anchor.depth) >> 2) * factor) >> 10));
      last_depth = signed_word(result.depth >> render_geometry::fraction_bits);
    } else {
      int const factor{interpolation >> 1};
      result.horizontal = signed_coordinate(anchor.horizontal + ((signed_word(signed_coordinate(current.horizontal - anchor.horizontal) >> 7) * factor) >> 8));
      result.vertical = signed_coordinate(anchor.vertical + ((signed_word(signed_coordinate(current.vertical - anchor.vertical) >> 7) * factor) >> 8));
      result.depth = signed_word((anchor.depth >> render_geometry::fraction_bits) + ((signed_word((last_depth - (anchor.depth >> render_geometry::fraction_bits)) * 2) * factor) >> 16)) * render_geometry::fraction_scale;
      vertices[cursor] = project_vertex(result, screen_origin);
    }
    camera_vertices[cursor] = result;
    defined[cursor++] = true;
  }

  void set(size_t const axis, size_t &position) {
    /// Replace one cached coordinate contribution from its signed operand
    projection.set_component(axis, std::bit_cast<int16_t>(word(position)));
  }

  uint8_t colour(uint8_t const source) const noexcept {
    /// 3383 resolves the supplied distance shade table and dynamic colour codes
    unsigned int shade{static_cast<unsigned int>(source & model_colours::shade_mask)};
    if(shade < model_colours::shade_count) shade = colours.shades[shade];
    else {
      // special shade codes combine beacon light with the brightest distance shade, then compress into the normal ramp
      unsigned int const value{static_cast<uint8_t>((colours.dynamic & model_colours::shade_mask) + colours.shades.back())};
      shade = std::min(model_colours::shade_count - 1, static_cast<unsigned int>(static_cast<uint8_t>(value + 11 - (value >> 2))) >> 1);
    }
    return static_cast<uint8_t>((source & model_colours::ramp_mask) + shade);
  }

public:
  interpreter(framework::render::indexed_surface frame, std::span<std::byte const> const bytes,
    projection_parameters const parameters, model_colours const &palette, raster_viewport const bounds, model_path const drawing_path, model_animation const &animation_state, model_shading const shading_mode)
    : target{frame}, pool{bytes}, projection{parameters}, colours{palette}, animation{animation_state}, distance_high{static_cast<uint8_t>(parameters.depth.whole >> 8)}, viewport{bounds}, path{drawing_path}, shading{shading_mode}, screen_origin{parameters.origin} {
    /// Keep bytecode, projection cache and vertex storage together for one drawing invocation
  }

  void run(size_t position, unsigned int const depth = 0) {
    /// Follow the original drawing commands with either direct or near-clipped vertex storage
    unsigned int constexpr maximum_call_depth{40};
    unsigned int constexpr maximum_instructions{10000};                        // malformed-stream guards, not original game limits
    if(depth > maximum_call_depth) throw std::invalid_argument{"Model call nesting exceeds the supported bound"};
    while(true) {
      if(++instructions > maximum_instructions) throw std::invalid_argument{"Model instruction limit exceeded"};
      auto const at{position};
      auto const opcode{static_cast<model_opcode>(byte(position))};
      switch(opcode) {
      case model_opcode::return_from_model:
        return;
      case model_opcode::set_interpolation_and_anchor:
        interpolation = static_cast<uint16_t>(parameter(position));
        save_anchor();
        break;
      case model_opcode::save_anchor:
        save_anchor();
        break;
      case model_opcode::emit_interpolated:
        emit_interpolated();
        break;
      case model_opcode::set_animated_c:
        projection.set_component(2, parameter(position));
        break;
      case model_opcode::disc:
        {
          auto const index{colour(byte(position))};
          auto const radius{word(position)};
          if(cursor == 0) throw std::invalid_argument{"Model disc has no preceding vertex"};
          --cursor;
          auto const centre{camera_vertices[cursor]};
          auto const depth{path == model_path::near_clipped ? signed_word(centre.depth >> render_geometry::fraction_bits) : last_depth};
          if(path == model_path::near_clipped && depth < render_geometry::near_depth) break;
          if(depth == 0) throw std::domain_error{"Model disc has zero depth"};
          auto const point{path == model_path::near_clipped ? project_vertex(centre, screen_origin) : vertices[cursor]};
          draw_disc(target, {point.x, point.y}, radius / static_cast<uint16_t>(depth), index, viewport);
        }
        break;
      case model_opcode::line:
        {
          auto const index{colour(byte(position))};
          auto const a{byte(position)};
          auto const b{byte(position)};
          if(!defined[a] || !defined[b]) throw std::invalid_argument{"Model line references an undefined vertex"};
          screen_vertex first{}, last{};
          if(path == model_path::near_clipped) {
            auto const left{camera_vertices[a]}, right{camera_vertices[b]};
            bool const left_inside{left.depth >= render_geometry::near_depth_fixed}, right_inside{right.depth >= render_geometry::near_depth_fixed};
            if(!left_inside && !right_inside) break;
            first = left_inside ? project_vertex(left, screen_origin) : near_intersection(right, left, screen_origin);
            last = right_inside ? project_vertex(right, screen_origin) : near_intersection(left, right, screen_origin);
          } else {
            first = vertices[a];
            last = vertices[b];
          }
          draw_world_line(target, {first.x, first.y}, {last.x, last.y}, index, viewport);
        }
        break;
      case model_opcode::call:
      case model_opcode::jump:
        {
          auto const operand{position};
          auto const destination{relative(operand, word(position))};
          if(opcode == model_opcode::call) run(destination, depth + 1);
          else position = destination;
        }
        break;
      case model_opcode::reset_vertex_cursor:
        cursor = 0;
        break;
      case model_opcode::set_vertex_cursor:
        {
          auto const address{word(position)};
          unsigned int constexpr native_vertex_buffer{0xfc00};
          unsigned int constexpr projected_vertex_bytes{4};                    // two 16-bit screen coordinates
          if(address < native_vertex_buffer || (address % projected_vertex_bytes)) throw std::invalid_argument{"Invalid projected vertex cursor"};
          cursor = (address - native_vertex_buffer) / projected_vertex_bytes;
        }
        break;
      case model_opcode::zero_c:
        projection.zero_component(2);
        break;
      case model_opcode::zero_a:
        projection.zero_component(0);
        break;
      case model_opcode::zero_b:
        projection.zero_component(1);
        break;
      case model_opcode::negate_c:
        projection.negate_component(2);
        break;
      case model_opcode::negate_a:
        projection.negate_component(0);
        break;
      case model_opcode::negate_b:
        projection.negate_component(1);
        break;
      case model_opcode::zero_c_and_emit:
        projection.zero_component(2);
        emit();
        break;
      case model_opcode::zero_a_and_emit:
        projection.zero_component(0);
        emit();
        break;
      case model_opcode::zero_b_and_emit:
        projection.zero_component(1);
        emit();
        break;
      case model_opcode::negate_c_and_emit:
        projection.negate_component(2);
        emit();
        break;
      case model_opcode::negate_a_and_emit:
        projection.negate_component(0);
        emit();
        break;
      case model_opcode::negate_b_and_emit:
        projection.negate_component(1);
        emit();
        break;
      case model_opcode::set_bc_and_emit:
        set(1, position);
        set(2, position);
        emit();
        break;
      case model_opcode::set_c_and_emit:
        set(2, position);
        emit();
        break;
      case model_opcode::set_ca_and_emit:
        set(2, position);
        set(0, position);
        emit();
        break;
      case model_opcode::set_a_and_emit:
        set(0, position);
        emit();
        break;
      case model_opcode::set_cab_and_emit:
        set(2, position);
        set(0, position);
        set(1, position);
        emit();
        break;
      case model_opcode::set_ab_and_emit:
        set(0, position);
        set(1, position);
        emit();
        break;
      case model_opcode::set_b_and_emit:
        set(1, position);
        emit();
        break;
      case model_opcode::emit:
        emit();
        break;
      case model_opcode::flat_polygon:
      case model_opcode::flat_triangle:
      case model_opcode::flat_quad:
      case model_opcode::shaded_triangle:
      case model_opcode::shaded_quad:
      case model_opcode::shaded_polygon:
        {
          // fixed triangle/quad opcodes encode count minus one; shaded forms add 12, variable polygons store it as an operand
          bool const shaded{opcode >= model_opcode::shaded_triangle};
          unsigned int const count{static_cast<unsigned int>(opcode == model_opcode::flat_polygon || opcode == model_opcode::shaded_polygon ? byte(position) : shaded ? std::to_underlying(opcode) - 12 : std::to_underlying(opcode)) + 1};
          auto const source_colour{byte(position)};
          auto const index{colour(source_colour)};
          std::array<screen_vertex, clipped_polygon_vertex_limit> face{};
          std::array<shaded_vertex, clipped_polygon_vertex_limit> shaded_face{};
          std::array<uint16_t, 256> vertex_shades{};
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
                // high byte is the palette index; low byte starts at half a shade plus shade for native interpolation bias
                vertex_shades[i] = static_cast<uint16_t>(((source_colour & model_colours::ramp_mask) + shade) * 256 + shade + 128);
                shaded_face[i] = {
                  .position{face[i].x, face[i].y},
                  .shade{vertex_shades[i]}
                };
              }
            }
          }
          if(shaded && shading == model_shading::gouraud) {
            auto const projected_count{path == model_path::near_clipped
              ? clip_near_shaded_polygon(std::span{camera_face}.first(count), std::span{vertex_shades}.first(count), screen_origin, shaded_face) : count};
            draw_gouraud_polygon(target, std::span{shaded_face}.first(projected_count), viewport);
            break;
          }
          auto const projected_count{path == model_path::near_clipped
            ? clip_near_polygon(std::span{camera_face}.first(count), screen_origin, face) : count};
          draw_flat_polygon(target, std::span{face}.first(projected_count), index, viewport);
        }
        break;
      case model_opcode::skip_back_facing:
        {
          std::array<uint8_t, 3> const indices{byte(position), byte(position), byte(position)};
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
      case model_opcode::skip_near:
        {
          auto const threshold{byte(position)};
          auto const skip{byte(position)};
          if(static_cast<uint8_t>(distance_high - threshold) & 128) position += skip;
        }
        break;
      default:
        throw std::invalid_argument{std::format("Unsupported flat model opcode {:02x} at pool offset {:04x}", std::to_underlying(opcode), at)};
      }
    }
  }
};

} // anonymous namespace

void update_fountain_parameters(model_animation &animation, uint16_t const clock) noexcept {
  /// DB1A resets six fountain heights and offsets the two active bands using the original sine table
  std::fill_n(animation.parameters.begin() + 9, 6, static_cast<int16_t>(-384));
  std::array<int, 7> constexpr amplitudes{118, 154, 136, 112, 85, 60, 118};    // original bytes at DB6A
  auto const phase{static_cast<uint32_t>(static_cast<uint16_t>(clock << 5)) * 6};
  auto const band{phase >> 16};
  auto const sine{maths::original_sine[(phase & 65535) >> 7]};
  animation.parameters[9 + band] += static_cast<int16_t>((amplitudes[band] * sine) >> 16);
  animation.parameters[9 + (band + 1) % 6] -= static_cast<int16_t>((2 * amplitudes[band + 1] * sine) >> 16);
}

void draw_model(framework::render::indexed_surface target, std::span<std::byte const> const pool,
  size_t const model_offset, projection_parameters const projection, model_colours const &colours, raster_viewport const viewport, model_path const path, model_animation const &animation, model_shading const shading) {
  /// The common eleven-byte model header precedes drawing code for both city and special definitions
  if(model_offset > pool.size() || pool.size() - model_offset < 12) throw std::invalid_argument{"Model has no complete header and drawing body"};
  if(!viewport.fits(target.width, target.height)) throw std::invalid_argument{"Model viewport exceeds the framebuffer"};
  interpreter{target, pool, projection, colours, viewport, path, animation, shading}.run(model_offset + 11);
}

} // namespace darker::graphics
