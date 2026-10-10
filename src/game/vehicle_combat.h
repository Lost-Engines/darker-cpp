#pragma once

#include <cstdint>
#include <span>
#include "game/city_map.h"
#include "game/projectile_pool.h"
#include "game/scenario_actor.h"
#include "game/time.h"

namespace darker::game {

projectile *fire_vehicle_missile(projectile_pool &pool, scenario_actor &vehicle, object_pose const &player,
  city_map const &cells, std::span<resources::city_type const> types, uint8_t direction, clock_tick clock, uint8_t difficulty, uint16_t model);

} // namespace darker::game
