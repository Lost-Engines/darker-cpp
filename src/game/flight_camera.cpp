#include "game/flight_camera.h"
#include <algorithm>
#include <array>
#include <bit>
#include <stdexcept>
#include "game/angular_motion.h"
#include "maths/direction.h"
#include "maths/sine_table.h"

namespace darker::game {
namespace {

std::int16_t word(int const value) noexcept {
  /// Retain the camera's signed word boundaries
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

} // namespace

void flight_camera::drop(camera_mode const selected, object_pose const &player) noexcept {
  /// 260D drops the camera at the craft's whole-word position and clears the look offsets
  mode = selected;
  anchor = player;
  anchor.fractions = {};
  anchor.angles.roll = 0;
  look_heading = 0;
  look_pitch = 0;
  looking = false;
}

void flight_camera::update_look(flight_steering const drive, bool const held, std::uint16_t const frame_step, bool const landed) noexcept {
  /// 7872 redirects steering into look offsets; 7CA4 returns the released view along its original maximum-axis vector
  bool const was_looking{looking};
  if(held) {
    look_heading = static_cast<std::uint16_t>(look_heading - drive.bank * 4);
    look_pitch = static_cast<std::uint16_t>(look_pitch + drive.pitch * 4);
    if(landed && static_cast<int>(mode) < 3) {
      auto const angle{word(look_heading + 32768)};
      auto const magnitude{static_cast<std::uint16_t>(angle < 0 ? ~angle : angle)};
      look_pitch = static_cast<std::uint16_t>((magnitude >> 3) - 0x1500);
    }
  } else if(frame_step != 0 && mode != camera_mode::fixed) {
    int const heading{word(look_heading)}, pitch{word(look_pitch)};
    auto const x{static_cast<unsigned int>(heading < 0 ? -heading : heading)};
    auto const y{static_cast<unsigned int>(pitch < 0 ? -pitch : pitch)};
    auto const major{std::max(x, y)};
    auto const step{static_cast<unsigned int>(static_cast<std::uint16_t>(frame_step * 128))};
    auto const remaining{major > step ? major - step : 0};
    auto const divisor{std::max(major, step)};
    if(divisor != 0) {
      auto const hx{static_cast<int>(x * remaining / divisor)}, py{static_cast<int>(y * remaining / divisor)};
      look_heading = static_cast<std::uint16_t>(heading < 0 ? -hx : hx);
      look_pitch = static_cast<std::uint16_t>(pitch < 0 ? -py : py);
    }
  }
  looking = held || look_heading != 0 || look_pitch != 0;
  if(was_looking && !looking && mode == camera_mode::cockpit) distance = 0x8000;
}

camera_mode flight_camera::visible_mode() const noexcept {
  /// Looking from the cockpit temporarily selects the original behind-craft view
  return mode == camera_mode::cockpit && looking ? camera_mode::behind : mode;
}

object_pose flight_camera::view(object_pose const &player, std::uint16_t const frame_step, bool const landed, camera_subject const subject, bool const underground) {
  /// Translate the ordinary player views at 24A4/24A7/24E6 and the following-distance path 254D
  if(distance_step >= 6) throw std::out_of_range{"Following camera has six distance settings"};
  bool const absent{subject == camera_subject::absent_object};
  object_pose result{absent ? anchor : player};
  if(mode == camera_mode::tracking || mode == camera_mode::fixed) {
    result = anchor;
    if(mode == camera_mode::tracking) {
      auto const direction{maths::direction_from_displacement({static_cast<std::uint16_t>(anchor.position.column - player.position.column),
        static_cast<std::uint16_t>(anchor.position.row - player.position.row), static_cast<std::uint16_t>(player.position.height - anchor.position.height)})};
      result.angles = {
        .heading{direction.heading},
        .pitch{direction.pitch},
        .roll{0}
      };
    }
  }
  // 24A7 sends underground player following views straight to 24E6, preserving roll and distance.
  auto const visible{visible_mode()};
  auto const active{underground && subject == camera_subject::player
    && (visible == camera_mode::behind || visible == camera_mode::level) ? camera_mode::fullscreen : visible};
  bool const missile{subject == camera_subject::missile || subject == camera_subject::missile_effect};
  bool const object{subject == camera_subject::object || subject == camera_subject::object_effect};
  bool const following{(active == camera_mode::object && !absent) || active == camera_mode::behind || active == camera_mode::level || (missile && (active == camera_mode::cockpit || (subject == camera_subject::missile_effect && active == camera_mode::fullscreen)))};
  if(subject == camera_subject::missile_effect && active == camera_mode::fullscreen && (distance & 0x8000)) distance = 0x200;
  result.angles.heading = static_cast<std::uint16_t>(result.angles.heading + look_heading);
  result.angles.pitch = static_cast<std::uint16_t>(result.angles.pitch + look_pitch);
  if(active == camera_mode::level) result.angles.roll = 0;
  if(following && missile) result.angles.pitch = static_cast<uint16_t>(result.angles.pitch - 0x800);
  if(following && landed && !missile) result.angles.pitch = static_cast<std::uint16_t>(result.angles.pitch - 1024);
  normalise_attitude(result.angles);
  for(auto *angle : {&result.angles.heading, &result.angles.pitch, &result.angles.roll})
    *angle = static_cast<std::uint16_t>(static_cast<std::uint16_t>(*angle + 15) & 0xffc0);
  if(!following) return result;
  std::array<int, 6> constexpr steps{6, 9, 13, 18, 26, 34};
  constexpr std::array<int,6> object_effect_steps{10,11,13,18,29,34};
  constexpr std::array<int,6> missile_steps{3,5,7,10,14,18}, effect_steps{10,11,12,14,16,18};
  auto const &distances{subject == camera_subject::object_effect ? object_effect_steps : subject == camera_subject::missile ? missile_steps : subject == camera_subject::missile_effect ? effect_steps : steps};
  int const target{landed && !missile && !object ? 0x440 : distances[distance_step] * 256};
  if(distance & 0x8000) distance = static_cast<std::uint16_t>(target);
  else {
    int const old{word(distance)};
    int const next{word(old + (old < target ? frame_step * 8 : -frame_step * 8))};
    distance = static_cast<std::uint16_t>(old < target ? std::min(next, target) : std::max(next, target));
  }
  auto pitch{result.angles.pitch >> 6};
  auto const heading{result.angles.heading >> 6};
  int const vertical{(word(distance) * maths::original_sine[pitch]) >> 16};
  auto height{word(result.position.height + (distance >> 4) - vertical)};
  if(height < 82 && vertical != 0 && !landed) {
    auto const index{maths::direction_index(static_cast<std::uint16_t>((vertical + pitch * 2) * 2), distance)};
    pitch = index >> 1;
    result.angles.pitch = static_cast<std::uint16_t>(index * 32);
    height = 82;
  }
  result.position.height = static_cast<std::uint16_t>(height);
  int const horizontal{((distance >> 2) * maths::original_sine[(pitch + 256) % 1024]) >> 16};
  displace_object(result, 0, (horizontal * maths::original_sine[heading]) >> 8);
  displace_object(result, 1, (horizontal * maths::original_sine[(heading + 256) % 1024]) >> 8);
  return result;
}

} // namespace darker::game
