#pragma once

#include <optional>
#include "audio/fm_stream.h"
#include "game/mission_combat.h"

namespace darker::audio {

std::optional<uint16_t> audible_level(std::array<uint16_t, 3> source, std::array<uint16_t, 3> listener,
  uint16_t level, uint8_t flags) noexcept;
uint16_t doppler_factor(game::object_pose const *motion, uint16_t heading, uint16_t pitch) noexcept;
uint16_t spatial_pitch(uint16_t pitch, std::array<uint16_t, 3> source, game::object_pose const &listener,
  game::object_pose const *source_motion) noexcept;

struct object_sound_state {
  uint16_t identity{0};
  uint8_t flags{0};
  uint16_t damage{0};
  uint8_t fade{0};
  uint16_t deadline{0};
};

fm_note object_sound(game::object_definition const &definition, game::object_pose const &pose,
  object_sound_state state, uint16_t clock);

class world_sounds {
private:
  std::array<uint64_t, 9> owners{};
  std::array<uint16_t, 9> generations{};

public:
  fm_frame mix(fm_frame const &player, game::mission_combat const &combat, game::object_pose const &listener, uint16_t clock = 0);
};

} // namespace darker::audio
