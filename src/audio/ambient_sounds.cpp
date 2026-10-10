#include "audio/ambient_sounds.h"
#include <bit>
#include <stdexcept>
#include "game/beacon_light.h"
#include "game/radar_coverage.h"
#include "maths/world_coordinates.h"

namespace darker::audio {
namespace {

bool reached(uint16_t const clock, uint16_t const deadline) noexcept {
  /// Fixed-record timing uses the sign of a wrapping word subtraction
  return std::bit_cast<int16_t>(static_cast<uint16_t>(clock - deadline)) >= 0;
}

bool region(ambient_source &source, ambient_context const &context, uint8_t const x, uint8_t const y,
  std::array<uint16_t, 4> const sites) {
  /// 36E2 selects one proxy emitter from listener quadrants, retaining each low coordinate byte
  auto const site{sites[((context.listener.column >> 8) >= x ? 2 : 0) + ((context.listener.row >> 8) < y ? 1 : 0)]};
  if(site & 0x8000) return false;
  source.sound.position.column = static_cast<uint16_t>((source.sound.position.column & 255) + (site & 255) * 256);
  source.sound.position.row = static_cast<uint16_t>((source.sound.position.row & 255) + (site & 0xff00));
  return true;
}

bool callback(ambient_source &source, ambient_context const &context, game::city_map const &cells) {
  /// Reconstruct the ten Delphi fixed-record callbacks before the common timer/admission path
  auto &sound{source.sound};
  auto const periodic{[&]{
    if(reached(context.clock, sound.deadline)) {
      sound.deadline = static_cast<uint16_t>(sound.deadline + sound.definition.duration * 16);
      sound.definition.flags &= 0xd7;
    }
  }};
  auto const position{[&](uint8_t const column, uint8_t const row){
    sound.position.column = static_cast<uint16_t>((sound.position.column & 255) + column * 256);
    sound.position.row = static_cast<uint16_t>((sound.position.row & 255) + row * 256);
  }};
  switch(source.callback) {
  case 0x3685: {
    auto const centre{game::make_radar_coverage(cells, context.listener, true).centre};
    if((centre[0] | centre[1]) & 128) return false;
    if(cells[centre[1] * 128 + centre[0]].state & 0xe0) return false;
    if(context.changes & 0x200) {
      position(centre[0], centre[1]);
      sound.deadline = static_cast<uint16_t>(context.clock + sound.definition.duration * 16);
      sound.definition.flags &= 0xd7;
    }
    return true;
  }
  case 0x372f: {
    auto const centre{game::beacon_grid_cell(context.listener)};
    if((centre[0] | centre[1]) & 128) return false;
    auto const cell{cells[centre[1] * 128 + centre[0]]};
    if(cell.type != 1) return false;
    sound.definition.level = static_cast<uint16_t>(218 * cell.state);
    position(centre[0], centre[1]);
    return true;
  }
  case 0x36b9:
    if(context.supplementary || !region(source, context, 64, 88, {0xffff, 0x0f15, 0x7073, 0x3a74})) return false;
    periodic();
    return true;
  case 0x36d8:
    return !context.supplementary && region(source, context, 64, 88, {0xffff, 0x0c17, 0x706f, 0x3b6f});
  case 0x365f: {
    auto const phase{static_cast<uint16_t>(context.clock >> 2)};
    auto const high{static_cast<uint8_t>(~(phase >> 8))};
    auto low{static_cast<uint8_t>(high & 3 ? 0 : phase)};
    if(high & 4) low = static_cast<uint8_t>(~low);
    sound.definition.pitch = static_cast<uint16_t>((low >> 2) + 0x1b1);
    return region(source, context, 40, 26, {0x2c17, 0x0d1f, 0x2632, 0x0f34});
  }
  case 0x3703:
    if(!context.gate_active) return false;
    position(static_cast<uint8_t>((context.gate_site & 255) >> 1), static_cast<uint8_t>(context.gate_site >> 8));
    return true;
  case 0x36c1:
    periodic();
    return true;
  default:
    throw std::invalid_argument{"Unknown ambient sound callback"};
  }
}

} // anonymous namespace

std::array<ambient_source, 10> make_ambient_sources(uint16_t const clock) {
  /// Original records 37A6/37BA/380A–3896 retain their stored durations, flags and fixed positions
  struct record {
    uint16_t pitch;
    uint8_t duration, flags;
    maths::world_position position;
    uint16_t level, callback;
    uint8_t patch;
  };
  std::array<record, 10> constexpr records{{
    {13856, 7, 1, {128, 128, 1664}, 47104, 0x3685, 30},
    {866, 0, 41, {128, 128, 2400}, 0, 0x372f, 0},
    {650, 120, 1, {128, 128, 0}, 65535, 0x36b9, 11},
    {866, 0, 41, {128, 128, 0}, 61952, 0x36d8, 10},
    {459, 0, 41, {128, 128, 0}, 45568, 0x365f, 21},
    {385, 0, 41, {128, 128, 0}, 53760, 0x3703, 25},
    {1300, 64, 1, {14464, 16768, 1024}, 49664, 0x36c1, 1},
    {918, 67, 1, {15744, 16768, 1024}, 49664, 0x36c1, 1},
    {770, 70, 1, {14464, 18048, 1024}, 49664, 0x36c1, 1},
    {650, 73, 1, {15744, 18048, 1024}, 49664, 0x36c1, 1},
  }};
  std::array<ambient_source, 10> result{};
  for(size_t i{0}; i < records.size(); ++i) {
    auto const &r{records[i]};
    result[i] = {
      .sound{
        .position{r.position},
        .definition{
          .duration{r.duration},
          .pitch{r.pitch},
          .level{r.level},
          .patch{r.patch},
          .flags{r.flags}
        },
        .deadline{clock}
      },
      .callback{r.callback}
    };
  }
  return result;
}

bool advance_ambient_source(ambient_source &source, ambient_context const &context,
  game::city_map const &cells, bool const was_playing) {
  /// 3599 runs callbacks before timer and previous-voice checks, resetting rejected deadlines to now
  auto &sound{source.sound};
  if(!callback(source, context, cells)
    || (sound.definition.duration && (reached(context.clock, sound.deadline) || ((sound.definition.flags & 8) && !was_playing)))) {
    sound.deadline = context.clock;
    return false;
  }
  if(sound.definition.duration && !(sound.definition.flags & 8)) {
    sound.definition.flags |= 0x28;
    ++source.generation;
  }
  return true;
}

std::vector<game::effect_sound> ambient_sounds::advance(ambient_context const &context, game::city_map const &cells,
  uint16_t const playing_mask, uint8_t const world_mode) {
  /// BDEC excludes these Delphi records from both Halon and underground fixed-record ranges
  std::vector<game::effect_sound> result;
  if(world_mode != 0) return result;
  for(size_t i{0}; i < sources.size(); ++i) {
    auto &source{sources[i]};
    if(!advance_ambient_source(source, context, cells, (playing_mask & (1u << i)) != 0)) continue;
    source.sound.identity = static_cast<uint32_t>(i * 65536 + source.generation);
    result.push_back(source.sound);
  }
  return result;
}

} // namespace darker::audio
