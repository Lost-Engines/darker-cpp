#include "game/vehicle_route.h"
#include <array>
#include <bit>
#include <stdexcept>
#include <utility>
#include "game/random.h"
#include "maths/sine_table.h"

namespace darker::game {
namespace {

std::array<uint16_t,2> curve(uint16_t const angle, uint8_t const fraction) {
  /// 914A interpolates the native quarter-circle tables using byte differences and an eight-bit weight
  auto const index{static_cast<size_t>(angle >> 6)};
  auto const cosine{maths::original_sine[index + 256]}, sine{maths::original_sine[index]};
  auto const cosine_delta{static_cast<uint8_t>(cosine - maths::original_sine[index + 257])};
  auto const sine_delta{static_cast<uint8_t>(maths::original_sine[index + 1] - sine)};
  return {static_cast<uint16_t>(sine + ((sine_delta * fraction) >> 8)),
    static_cast<uint16_t>(cosine - ((cosine_delta * fraction) >> 8))};
}

} // namespace

vehicle_route_result advance_vehicle_route(vehicle_route &route, object_pose &pose, uint8_t &flags,
  std::span<std::byte const> const program, uint16_t const clock, int16_t const model_height, uint16_t &random_state) {
  /// 8F3B advances one timed route unit per callback, preserving byte coordinates and wrapping clock arithmetic
  vehicle_route_result result;
  uint16_t height{0};
  auto direction{static_cast<unsigned int>((std::rotl(pose.angles.heading,3) + 1) & 6)};
  uint16_t const elapsed{static_cast<uint16_t>(clock - route.origin)};
  uint8_t lookahead{0};
  if(elapsed & 0xf000) {
    route.effect_progress = 0;
    route.origin = static_cast<uint16_t>(route.origin + 0x1000);
    if(route.command >= 16) route.command -= 16;
    else {
      size_t branches{0};
      for(;;) {
        if(route.cursor >= program.size()) throw std::out_of_range{"Vehicle route exceeds shared scenario data"};
        route.command = std::to_integer<uint8_t>(program[route.cursor++]);
        lookahead = route.cursor < program.size() ? std::to_integer<uint8_t>(program[route.cursor]) : 0;
        if(route.command != 255) break;
        if(route.cursor >= program.size() || ++branches > program.size()) throw std::invalid_argument{"Invalid vehicle route branch chain"};
        auto const target{static_cast<ptrdiff_t>(route.cursor) + std::bit_cast<int8_t>(lookahead)};
        if(target < 0 || static_cast<size_t>(target) >= program.size()) throw std::out_of_range{"Vehicle route branch leaves shared scenario data"};
        route.cursor = static_cast<size_t>(target);
      }
    }
    if(flags & 2) {
      pose.angles.heading = std::rotr(static_cast<uint16_t>(direction),3);
      constexpr std::array<int,4> columns{0,-1,0,1}, rows{-1,0,1,0};
      pose.position.column = static_cast<uint16_t>(pose.position.column + columns[direction / 2] * 256);
      pose.position.row = static_cast<uint16_t>(pose.position.row + rows[direction / 2] * 256);
    }
  }
  uint16_t along{static_cast<uint16_t>(elapsed << 4)}, across{0x8000};
  pose.speed = 64;
  pose.angles.pitch = 0;
  flags |= 2;
  switch(route.command & 15) {
  case 0:
    along = 0;
    flags &= 0xfd;
    pose.speed = 0;
    break;
  case 1:
    break;
  case 2:
    {
      auto fraction{static_cast<uint8_t>(along)};
      auto angle{static_cast<uint16_t>((along & 0xff00) >> 2)};
      pose.angles.heading = static_cast<uint16_t>(((pose.angles.heading - 1) | 0x3fff) - angle);
      bool const second_half{((std::rotl(pose.angles.heading,3) ^ direction) & 6) != 0};
      if(!second_half) { angle ^= 0x3fc0; fraction ^= 0xf0; }
      auto const point{curve(angle,fraction)};
      along = second_half ? point[0] : static_cast<uint16_t>(~point[0]);
      across = point[1];
    }
    break;
  case 3:
    {
      uint8_t const fraction{static_cast<uint8_t>(along)};
      uint16_t const angle{static_cast<uint16_t>((along & 0xff00) >> 2)};
      pose.angles.heading = static_cast<uint16_t>((pose.angles.heading & 0xc000) + angle);
      auto const point{curve(angle,fraction)};
      along = point[0];
      across = static_cast<uint16_t>(~point[1]);
      direction = std::rotl(pose.angles.heading,3) & 6;
    }
    break;
  case 4:
  case 5:
    {
      bool const descending{(route.command & 15) == 5};
      if(descending && route.effect_progress == 0) {
        ++route.effect_progress;
        result.damage_cell = static_cast<uint16_t>((pose.position.column >> 8) | (pose.position.row & 0xff00));
      }
      // 905C folds the route phase, then derives a triangular pitch and a linear height profile.
      uint16_t const half_width{static_cast<uint16_t>(descending ? 0x3100 : 0x3400)};
      uint16_t const maximum_height{static_cast<uint16_t>(descending ? 160 : 32)};
      uint16_t const maximum_pitch{static_cast<uint16_t>(descending ? 2438 : 868)};
      auto const folded{static_cast<uint16_t>(along & 0x8000 ? ~along : along)};
      auto const phase{static_cast<uint16_t>(folded - (descending ? 0x2a00 : 0x4800) + half_width)};
      if(phase < half_width*2) {
        auto const triangle{phase < half_width ? phase : half_width*2 - phase};
        auto const pitch{static_cast<uint16_t>(triangle*maximum_pitch / half_width)};
        pose.angles.pitch = static_cast<uint16_t>(along & 0x8000 ? -pitch : pitch);
        height = static_cast<uint16_t>(phase*maximum_height / (half_width*2));
      } else {
        height = std::bit_cast<int16_t>(static_cast<uint16_t>(phase - half_width)) < 0 ? 0 : maximum_height;
      }
    }
    break;
  case 6:
  case 7:
    {
      if((route.command & 15) == 6 && route.effect_progress == 0) {
        ++route.effect_progress;
        result.damage_cell = static_cast<uint16_t>((pose.position.column >> 8) | (pose.position.row & 0xff00));
      }
      if(route.effect_progress < along) {
        auto const first{next_random(random_state)};
        auto const interval{(first >> 6) + (first >> 7) + 0x578};
        auto const next{static_cast<unsigned int>(along) + interval};
        route.effect_progress = static_cast<uint16_t>(next > 65535 ? 65535 : next);
        auto const second{next_random(random_state)};
        uint16_t a{static_cast<uint16_t>((second & 0xff00) | 0x60)};
        uint16_t d{static_cast<uint16_t>(((first >> 7) & 0xff00) | static_cast<uint8_t>(128 + (std::bit_cast<int8_t>(static_cast<uint8_t>(second)) >> 3)))};
        constexpr std::array<uint16_t,5> reflection{0xffff,0xffff,0,0,0xffff};
        a ^= reflection[direction / 2];
        d ^= reflection[direction / 2 + 1];
        if(direction & 2) std::swap(a,d);
        vehicle_route_effect effect{.position{static_cast<uint16_t>((pose.position.column & 0xff00) | (d & 255)),
          static_cast<uint16_t>((pose.position.row & 0xff00) | (a & 255)),static_cast<uint16_t>(256 + (second >> 8))}};
        bool const burst{route.effect_countdown < 40};
        route.effect_countdown = static_cast<uint8_t>(route.effect_countdown - 40);
        if(burst) {
          route.effect_countdown = static_cast<uint8_t>(first | 0x40);
          effect.recipe = 0x77c5;
          effect.position.height = 0xe0;
        } else {
          effect.phase = static_cast<uint8_t>((first >> 14) + 4);
          effect.sound_level = static_cast<uint16_t>(0xd000 + (first >> 5));
        }
        result.effect = effect;
      }
      along = 0;
      flags &= 0xfd;
      pose.speed = 0;
    }
    break;
  case 9:
    if(!(flags & 0x40)) {
      flags |= 0x40;
      result.deadline = static_cast<uint16_t>(clock + 4096);
    }
    flags = static_cast<uint8_t>((flags & 0xf7) | (along < 0xf000 ? 8 : 0));
    break;
  case 8:
    if(!(flags & 0x20)) {
      flags |= 0x20;
      result.deadline = static_cast<uint16_t>(clock + 256);
    }
    break;
  default:
    throw std::invalid_argument{"Unknown vehicle route action"};
  }
  auto const action{route.command & 15};
  if(action == 0 || action == 1 || action == 6 || action == 7) result.firing_direction = static_cast<uint8_t>(direction);
  constexpr std::array<uint16_t,5> reflection{0xffff,0xffff,0,0,0xffff};
  along ^= reflection[direction / 2];
  across ^= reflection[direction / 2 + 1];
  if(direction & 2) std::swap(along,across);
  pose.position.column = static_cast<uint16_t>((pose.position.column & 0xff00) | (across >> 8));
  pose.position.row = static_cast<uint16_t>((pose.position.row & 0xff00) | (along >> 8));
  pose.fractions.column = static_cast<uint8_t>(across & 0xf0);
  pose.fractions.row = static_cast<uint8_t>(along & 0xf0);
  pose.position.height = static_cast<uint16_t>(height - model_height);
  return result;
}

} // namespace darker::game
