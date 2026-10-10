#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include "game/city_map.h"
#include "resources/scenario.h"

namespace darker::game {

void apply_scenario_cells(city_map &cells, resources::scenario_record const &record);
void commit_beacon_queue(city_map &cells, std::span<std::byte const> queue);

struct world_objectives {
  size_t list{2};
  size_t cursor{0};
  std::optional<std::array<resources::scenario_cell_list, 2>> script_lists{};

  size_t replace(city_map &cells, std::span<std::byte const> program);
  void advance(city_map const &cells, resources::scenario_record const &record, uint8_t damage_mask);
  bool complete(resources::scenario_record const &record) const;
};

} // namespace darker::game
