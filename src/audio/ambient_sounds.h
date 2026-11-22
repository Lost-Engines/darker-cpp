#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include "maths/world_coordinates.h"
#include "game/city_map.h"
#include "game/effects.h"
#include "resources/world_profile.h"

namespace darker::audio {

struct ambient_context {
  maths::map_position listener{};
  uint16_t clock{0};
  uint16_t changes{0};
  uint16_t gate_site{0};
  bool gate_active{false};
  bool supplementary{false};
};

struct ambient_source {
  game::effect_sound sound{};
  uint16_t callback{0};
  uint16_t generation{0};
};

std::array<ambient_source, 10> make_ambient_sources(uint16_t clock = 0);
bool advance_ambient_source(ambient_source &source, ambient_context const &context,
  game::city_map const &cells, bool was_playing);

class ambient_sounds {
private:
  std::array<ambient_source, 10> sources{make_ambient_sources()};

public:
  std::vector<game::effect_sound> advance(ambient_context const &context, game::city_map const &cells,
    uint16_t playing_mask, resources::world_kind world_mode);
};

} // namespace darker::audio
