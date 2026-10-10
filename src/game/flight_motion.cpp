#include "game/flight_motion.h"
#include <bit>
#include "maths/sine_table.h"

namespace darker::game {
namespace {

int16_t signed_word(int const value) noexcept {
  /// Wrap to a word before signed products and midpoint shifts
  return std::bit_cast<int16_t>(static_cast<uint16_t>(value));
}

int16_t approach_velocity(uint16_t &velocity, uint16_t const target, uint16_t const timestep) noexcept {
  /// 8211/826E retain the CMP/ADC increment even when the signed error product is zero
  auto const old{velocity};
  auto const delta{(signed_word(target - old) * signed_word(timestep)) >> 16};
  velocity = static_cast<uint16_t>(old + delta + (delta == 0 ? 1 : 0));
  return signed_word(old + (signed_word(velocity - old) >> 1));
}

} // anonymous namespace

void advance_speed_motion(object_pose &pose, uint16_t const speed, uint16_t const frame_step) noexcept {
  /// 8597 integrates the old/new speed midpoint and projects it through the object's current pitch and heading
  auto const previous{signed_word(pose.speed)};
  auto const midpoint{signed_word(previous + (signed_word(speed - previous) >> 1))};
  pose.speed = speed;
  auto const time{signed_word((frame_step & 255) << 8)};
  auto const distance{signed_word((static_cast<int32_t>(midpoint) * time) >> 14)};
  unsigned int const pitch{static_cast<unsigned int>(pose.angles.pitch >> 6)};
  unsigned int const heading{static_cast<unsigned int>(pose.angles.heading >> 6)};
  auto const sine{[](unsigned int const angle){
    return maths::original_sine[angle];
  }};
  auto const cosine{[&](unsigned int const angle){
    return sine((angle + 256) % 1024);
  }};
  displace_object(pose, 2, (sine(pitch) * distance) >> 8);
  auto const horizontal{signed_word((cosine(pitch) * distance) >> 15)};
  displace_object(pose, 1, -((cosine(heading) * horizontal) >> 11));
  displace_object(pose, 0, -((sine(heading) * horizontal) >> 11));
}

void advance_horizontal_flight(object_pose &pose, uint16_t &velocity, uint16_t const target,
  uint16_t const timestep, uint16_t const heading, uint16_t const pitch) noexcept {
  /// 8208 projects the target by mid-step pitch, approaches velocity, then integrates both horizontal axes
  auto const cosine{maths::original_sine[((pitch >> 6) + 256) % 1024]};
  auto const projected{static_cast<uint16_t>((signed_word(target) * cosine) >> 15)};
  auto const midpoint{approach_velocity(velocity, projected, timestep)};
  auto const travel{signed_word(-((midpoint * signed_word(timestep)) >> 16))};
  auto const sine_heading{maths::original_sine[heading >> 6]};
  auto const cosine_heading{maths::original_sine[((heading >> 6) + 256) % 1024]};
  displace_object(pose, 0, (sine_heading * travel) >> 8);
  displace_object(pose, 1, (cosine_heading * travel) >> 8);
}

void advance_vertical_flight(object_pose &pose, uint16_t &velocity, uint16_t const target, uint16_t const timestep) noexcept {
  /// 826E integrates the midpoint velocity into the altitude word and fractional byte
  auto const midpoint{approach_velocity(velocity, target, timestep)};
  displace_object(pose, 2, (midpoint * signed_word(timestep)) >> 7);
}

void measure_flight_speed(object_pose &pose, uint16_t const horizontal, uint16_t const vertical) noexcept {
  /// 8254/92E6 produce the floor of the integer vector length, without floating-point rounding
  auto const x{signed_word(horizontal)};
  auto const y{signed_word(vertical)};
  auto remainder{static_cast<uint32_t>(x * x) + static_cast<uint32_t>(y * y)};
  uint32_t root{0};
  for(uint32_t bit{1u << 30}; bit != 0; bit >>= 2) {
    if(remainder >= root + bit) {
      remainder -= root + bit;
      root = (root >> 1) + bit;
    } else {
      root >>= 1;
    }
  }
  pose.speed = static_cast<uint16_t>(root);
}

} // namespace darker::game
