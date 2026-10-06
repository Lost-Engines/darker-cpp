#pragma once

#include <span>
#include "game/city_map.h"
#include "resources/geometry_bank.h"

namespace darker::game {

void pack_city_state(city_map const &cells, std::span<resources::city_type const> types, std::span<std::byte> packed);
void restore_city_state(city_map &cells, std::span<resources::city_type const> types, std::span<std::byte const> packed, uint8_t stage);

} // namespace darker::game
