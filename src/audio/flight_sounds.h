#pragma once

#include <array>
#include <cstdint>
#include "audio/fm_stream.h"
#include "game/player_flight.h"

namespace darker::audio {

enum class flight_sound { boost, charged, caero_switch, skimma_switch, shield_start, shield_ready };

class flight_sounds {
private:
  fm_frame voices{};
  std::array<std::uint16_t, 9> deadlines{};
  bool shield_ready{false};

public:
  void trigger(flight_sound effect, std::uint16_t clock) noexcept;
  fm_frame advance(game::player_flight const &player, std::uint16_t clock, bool ready) noexcept;
};

} // namespace darker::audio
