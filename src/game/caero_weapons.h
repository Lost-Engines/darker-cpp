#pragma once

#include <cstdint>
#include <optional>
#include "game/caero_energy.h"
#include "game/projectile_pool.h"

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
  uint16_t clock{0};
  uint16_t frame_step{0};
  uint16_t target{0xffff};
  bool underground{false};
  uint16_t trigger_mask{0x4016};
};

uint8_t pinner_direct_strength(bool underground) noexcept;

std::optional<uint8_t> chargeable_impact_strength(uint16_t deadline, uint16_t clock) noexcept;

caero_fire_result fire_caero_weapon(projectile_pool &pool, caero_energy_state &energy, uint16_t &charge, caero_fire_request request);

} // namespace darker::game
