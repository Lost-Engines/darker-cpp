#pragma once

#include <optional>
#include "audio/fm_stream.h"
#include "audio/voice_allocation.h"
#include "game/mission_combat.h"
#include "maths/view_basis.h"
#include "maths/world_coordinates.h"

namespace darker::audio {

std::optional<uint16_t> audible_level(maths::world_position source, maths::world_position listener,
  uint16_t level, uint8_t flags) noexcept;
uint16_t doppler_factor(game::object_pose const *motion, uint16_t heading, uint16_t pitch) noexcept;
uint16_t spatial_pitch(uint16_t pitch, maths::world_position source, game::object_pose const &listener,
  game::object_pose const *source_motion, game::object_pose const *listener_motion = nullptr) noexcept;

std::array<uint8_t, 2> stereo_attenuation(maths::world_position delta, maths::view_basis const &basis, uint16_t level) noexcept;

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
  voice_allocation voices;

public:
  fm_frame mix(fm_frame const &player, game::mission_combat &combat, game::object_pose const &listener, uint16_t clock = 0, std::span<game::effect_sound const> ambient = {}, game::object_pose const *listener_motion = nullptr, game::object_pose const *player_source = nullptr);
  uint16_t audible_ambient() const noexcept;
  uint16_t audible_player() const noexcept;
};

} // namespace darker::audio
