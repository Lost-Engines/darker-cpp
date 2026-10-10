#pragma once

#include <cstdint>

namespace darker::resources {

enum class scenario_configuration : std::uint8_t {
  delphi_skimma = 0,
  delphi_caero = 1,
  halon_skimma = 2,
  halon_upgraded_skimma = 3,
  underground_caero = 4,
};

constexpr scenario_configuration decode_scenario_configuration(std::uint8_t packed) noexcept {
  // The high nibble selects a tunnel map or scenario-specific setup, not the craft configuration.
  return static_cast<scenario_configuration>(packed & 15);
}

} // namespace darker::resources
