#pragma once

#include <array>
#include <utility>
#include "game/object_definition.h"
#include "resources/scenario_configuration.h"

namespace darker::game {

// indices are shared by native definitions and geometry-bank special models
struct object_catalogue {
  static unsigned int constexpr definition_count{33};
  static object_definition_index constexpr first_caero_weapon{0};
  static unsigned int constexpr caero_weapon_count{10};
  static object_definition_index constexpr first_skimma_weapon{10};
  static unsigned int constexpr skimma_weapon_count{3};
  static object_definition_index constexpr first_player{24};
  static unsigned int constexpr player_count{5};

  static constexpr object_definition_index player(resources::scenario_configuration configuration) noexcept {
    /// Preserve the configuration-to-definition mapping, including Delphi's final Skimma
    return static_cast<object_definition_index>(first_player + std::to_underlying(configuration));
  }

  static constexpr object_definition_index skimma_weapon(unsigned int slot) noexcept {
    /// Convert a validated zero-based cockpit slot to the shared model/definition index
    return static_cast<object_definition_index>(first_skimma_weapon + slot);
  }
};

struct skimma_weapon_specification {
  object_definition_index definition;
  uint8_t working_capacity;
  uint8_t reserve_capacity;
};

inline std::array<skimma_weapon_specification, object_catalogue::skimma_weapon_count> constexpr skimma_weapon_specifications{{
  {
    .definition{object_catalogue::skimma_weapon(0)},
    .working_capacity{14},
    .reserve_capacity{5}
  },
  {
    .definition{object_catalogue::skimma_weapon(1)},
    .working_capacity{8},
    .reserve_capacity{3}
  },
  {
    .definition{object_catalogue::skimma_weapon(2)},
    .working_capacity{10},
    .reserve_capacity{4}
  }
}};

static_assert(object_catalogue::first_caero_weapon + object_catalogue::caero_weapon_count == object_catalogue::first_skimma_weapon);
static_assert(object_catalogue::first_player + object_catalogue::player_count <= object_catalogue::definition_count);

} // namespace darker::game
