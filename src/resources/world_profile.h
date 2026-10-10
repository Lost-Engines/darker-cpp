#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>
#include "resources/scenario_configuration.h"

namespace darker::resources {

enum class world_kind : uint8_t { delphi, halon, underground };

struct world_profile {
  std::string_view name;
  unsigned int geometry_entry;
  unsigned int first_map_entry;
  uint8_t damage_mask;
  bool beacon_lighting;
  bool numbered_tunnel_maps;

  constexpr unsigned int map_entry(uint8_t packed_configuration) const noexcept {
    /// Only underground configurations use the high nibble to select a map
    return first_map_entry + (numbered_tunnel_maps ? packed_configuration >> 4 : 0);
  }
};

inline std::array<world_profile, 3> constexpr world_profiles{{
  {
    .name{"Delphi"},
    .geometry_entry{30},
    .first_map_entry{68},
    .damage_mask{0x20},
    .beacon_lighting{true},
    .numbered_tunnel_maps{false}
  },
  {
    .name{"Halon"},
    .geometry_entry{31},
    .first_map_entry{69},
    .damage_mask{0x60},
    .beacon_lighting{false},
    .numbered_tunnel_maps{false}
  },
  {
    .name{"Underground"},
    .geometry_entry{32},
    .first_map_entry{70},
    .damage_mask{0x60},
    .beacon_lighting{false},
    .numbered_tunnel_maps{true}
  }
}};

constexpr world_kind configuration_world(scenario_configuration configuration) noexcept {
  /// Craft and world are independent: the final Skimma flies in Delphi
  using enum scenario_configuration;
  return configuration == underground_caero ? world_kind::underground
    : configuration == halon_skimma || configuration == halon_upgraded_skimma ? world_kind::halon : world_kind::delphi;
}

constexpr world_profile const &profile(world_kind world) noexcept {
  /// Borrow the rules for a validated native world identifier
  return world_profiles[std::to_underlying(world)];
}

} // namespace darker::resources
