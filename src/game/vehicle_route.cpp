#include "game/vehicle_route.h"
#include <array>
#include <bit>
#include <stdexcept>
#include <utility>
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

void advance_vehicle_route(vehicle_route &route, object_pose &pose, uint8_t &flags,
  std::span<std::byte const> const program, uint16_t const clock, int16_t const model_height) {
  /// 8F3B advances one timed route unit per callback, preserving byte coordinates and wrapping clock arithmetic
  if(route.removed) return;
  auto direction{static_cast<unsigned int>((std::rotl(pose.angles[0],3) + 1) & 6)};
  uint16_t const elapsed{static_cast<uint16_t>(clock - route.origin)};
  uint8_t lookahead{0};
  if(elapsed & 0xf000) {
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
      pose.angles[0] = std::rotr(static_cast<uint16_t>(direction),3);
      constexpr std::array<int,4> columns{0,-1,0,1}, rows{-1,0,1,0};
      pose.position[0] = static_cast<uint16_t>(pose.position[0] + columns[direction / 2] * 256);
      pose.position[1] = static_cast<uint16_t>(pose.position[1] + rows[direction / 2] * 256);
    }
  }
  uint16_t along{static_cast<uint16_t>(elapsed << 4)}, across{0x8000};
  pose.speed = 64;
  pose.angles[1] = 0;
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
      pose.angles[0] = static_cast<uint16_t>(((pose.angles[0] - 1) | 0x3fff) - angle);
      bool const second_half{((std::rotl(pose.angles[0],3) ^ direction) & 6) != 0};
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
      pose.angles[0] = static_cast<uint16_t>((pose.angles[0] & 0xc000) + angle);
      auto const point{curve(angle,fraction)};
      along = point[0];
      across = static_cast<uint16_t>(~point[1]);
      direction = std::rotl(pose.angles[0],3) & 6;
    }
    break;
  case 8:
    route.removed = !(flags & 0x20);
    break;
  default:
    throw std::invalid_argument{"Vehicle route requires raised traversal, damage or effect callbacks"};
  }
  constexpr std::array<uint16_t,5> reflection{0xffff,0xffff,0,0,0xffff};
  along ^= reflection[direction / 2];
  across ^= reflection[direction / 2 + 1];
  if(direction & 2) std::swap(along,across);
  pose.position[0] = static_cast<uint16_t>((pose.position[0] & 0xff00) | (across >> 8));
  pose.position[1] = static_cast<uint16_t>((pose.position[1] & 0xff00) | (along >> 8));
  pose.fractions[0] = static_cast<uint8_t>(across & 0xf0);
  pose.fractions[1] = static_cast<uint8_t>(along & 0xf0);
  pose.position[2] = static_cast<uint16_t>(-model_height);
}

} // namespace darker::game
