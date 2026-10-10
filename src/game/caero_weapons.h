#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include "game/caero_energy.h"
#include "game/city_map.h"
#include "game/collision_box.h"
#include "game/projectile_pool.h"
#include "game/time.h"
#include "maths/world_coordinates.h"

namespace darker::game {

struct caero_fire_result {
  projectile *shot{nullptr};
  bool ready{false};
  uint8_t next_selection{0};
};

struct caero_fire_request {
  launch_emitter const &emitter;
  uint8_t selection{0};
  uint8_t player_flags{0};
  bool pressed{false};
  bool held{false};
  uint16_t model{0};
  clock_tick clock{0};
  game_duration frame_step{0};
  uint16_t target{0xffff};
  bool underground{false};
  bool released{false};
};

enum class diffuser_impact { rejected, gas, destroyed };

struct diffuser_state {
  uint16_t cell{0};
  clock_tick deadline{0};
  clock_tick sound_deadline{0};

  diffuser_impact hit(bool gas, collision_category category, uint8_t state, uint16_t target, clock_tick clock) noexcept;
};

uint8_t caero_weapon_strength(city_map const &cells, maths::world_position position, uint16_t victim);

uint8_t pinner_direct_strength(bool underground) noexcept;

std::optional<impact_strength> chargeable_impact_strength(clock_tick deadline, clock_tick clock) noexcept;

caero_fire_result fire_caero_weapon(projectile_pool &pool, caero_energy_state &energy, uint16_t &charge, caero_fire_request request);

} // namespace darker::game
