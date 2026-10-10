#pragma once

#include <cstdint>
#include "game/flight_controls.h"
#include "game/object_pose.h"
#include "game/time.h"

namespace darker::game {

enum class camera_mode { cockpit, behind, level, fullscreen, tracking, fixed, object };

enum class camera_subject { player, missile, missile_effect, object, object_effect, absent_object };

struct flight_camera {
  camera_mode mode{camera_mode::cockpit};
  uint8_t distance_step{1};
  uint16_t distance{0x8000};
  uint16_t look_heading{0};
  uint16_t look_pitch{0};
  bool looking{false};
  object_pose anchor{};

  void drop(camera_mode selected, object_pose const &player) noexcept;

  void update_look(flight_steering drive, bool held, game_duration frame_step, bool landed = false) noexcept;
  camera_mode visible_mode() const noexcept;

  object_pose view(object_pose const &player, game_duration frame_step, bool landed = false, camera_subject subject = camera_subject::player, bool underground = false);
};

} // namespace darker::game
