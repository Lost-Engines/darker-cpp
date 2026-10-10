#include "graphics/particles.h"
#include <algorithm>
#include <bit>
#include "graphics/particle_tables.h"
#include "maths/sine_table.h"

namespace darker::graphics {

std::vector<particle_point> project_emitter(game::particle_emitter const &emitter, model_placement const centre,
  camera_basis const &basis, screen_vertex const &origin) {
  /// 689F projects each ring sample and walks both arcs from the farthest accepted point towards the nearest
  vec3<int> const offsets{
    std::bit_cast<int16_t>(centre.horizontal.whole) * 256 + centre.horizontal.fraction,
    std::bit_cast<int16_t>(centre.vertical.whole) * 256 + centre.vertical.fraction,
    std::bit_cast<int16_t>(centre.depth.whole) * 256 + centre.depth.fraction,
  };
  std::array<int16_t projection_axis::*, 3> const members{&projection_axis::horizontal, &projection_axis::vertical, &projection_axis::depth};
  std::array<vec2<int>, 3> coefficients{};
  for(unsigned int i{0}; i < 3; ++i) {
    coefficients[i] = {((basis[1].*members[i] * std::bit_cast<int16_t>(emitter.radius)) >> 16) >> 2,
      ((basis[0].*members[i] * std::bit_cast<int16_t>(emitter.radius)) >> 16) >> 2};
  }
  auto const wrap24{[](int const value){ return std::bit_cast<int32_t>(static_cast<uint32_t>(value) << 8) >> 8; }};
  unsigned int index{static_cast<unsigned int>(emitter.angle >> 5)};
  std::vector<particle_point> points;
  size_t far{0}, near{0};
  for(unsigned int sample{0}; sample < (emitter.sampling & 255); ++sample) {
    index &= 2046;
    auto const sine{maths::original_sine[index >> 1]};
    auto const cosine{maths::original_sine[((index >> 1) + 256) % 1024]};
    vec3<int> v{};
    for(unsigned int i{0}; i < 3; ++i) v[i] = wrap24(offsets[i] + ((coefficients[i][0] * sine) >> 8) + ((coefficients[i][1] * cosine) >> 8));
    auto const depth{v[2] >> 8};
    if(depth >= 32) {
      points.push_back({
        .x{std::bit_cast<int16_t>(static_cast<uint16_t>(v[0] / depth + origin.x))},
        .y{std::bit_cast<int16_t>(static_cast<uint16_t>(v[1] / depth + origin.y))},
        .depth{static_cast<int16_t>(depth)}
      });
      if(depth > points[far].depth) far = points.size() - 1;
      if(depth < points[near].depth) near = points.size() - 1;
    }
    index += ((emitter.sampling >> 6) | 3) + 1;
  }
  if(points.size() < 2) return points;
  std::ranges::reverse(points);
  int const count{static_cast<int>(points.size())};
  int const f{count - 1 - static_cast<int>(far)}, n{count - 1 - static_cast<int>(near)};
  std::vector<particle_point> ordered;
  int sp{0}, si{f};
  while(true) {
    ordered.push_back(points[si++]);
    if(si >= count) {
      if(sp == n) break;
      while(sp != n) ordered.push_back(points[sp++]);
      break;
    }
    if(si == n) break;
  }
  si = f - 1;
  if(si < sp) si = count - 1;
  while(true) {
    ordered.push_back(points[si]);
    if(si == n) break;
    if(--si < sp) si = count - 1;
  }
  return ordered;
}

void draw_particle(framework::render::indexed_cockpit_framebuffer &target,
  framework::render::indexed_cockpit_framebuffer const &sheet, particle_point const point, uint8_t const phase, int const bottom) {
  /// 6AA6 selects discrete source artwork and masked rows, preserving even-X destination alignment
  if(phase >= 22 || point.depth < 32) return;
  if(point.depth >= 8190) {
    if(point.x >= 0 && point.x < 320 && point.y >= 0 && point.y < bottom) target.pixels[point.y * 320 + point.x] = particle_point_colours[phase];
    return;
  }
  int const table{point.depth < 1752 ? 0x6b9c : point.depth < 2454 ? 0x6b79 : point.depth < 3510 ? 0x6b5e : point.depth < 4914 ? 0x6b47 : 0x6b34};
  auto const at{[](int const address){ return particle_selectors[address - 0x6b34]; }};
  int const radius{at(table + 10)};
  int const x{point.x - radius}, y{point.y - radius};
  int const alignment{x & 2}, destination_x{x & ~1};
  int const record{table + 5 * (phase & 1)};
  int const source_x{2 * (at(0x6bc7 + at(record) + phase / 2) + at(record + 1 + alignment))};
  int const source_y{at(record + 2 + alignment)};
  for(int row{0}; row < radius * 2; ++row) {
    if(y + row < 0 || y + row >= bottom) continue;
    int const skip{at(table + 11 + row * 2)}, width{at(table + 12 + row * 2)};
    for(int column{skip}; column < skip + width; ++column) {
      if(destination_x + column < 0 || destination_x + column >= 320) continue;
      target.pixels[(y + row) * 320 + destination_x + column] = sheet.pixels[(source_y + row) * 320 + source_x + column];
    }
  }
}

} // namespace darker::graphics
