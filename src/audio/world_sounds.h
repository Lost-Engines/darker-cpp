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

class world_sounds {
private:
  std::array<uint64_t, 9> owners{};
  std::array<uint16_t, 9> generations{};

public:
  fm_frame mix(fm_frame const &player, game::mission_combat const &combat, game::object_pose const &listener);
};

} // namespace darker::audio
