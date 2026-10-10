#pragma once

#include <cstdint>

namespace darker::resources {

enum class scenario_configuration : uint8_t {
  delphi_skimma,
  delphi_caero,
  halon_skimma,
  halon_upgraded_skimma,
  underground_caero,
};

constexpr scenario_configuration decode_scenario_configuration(uint8_t packed) noexcept {
  // the high nibble selects a tunnel map or scenario-specific setup, not the craft configuration
  return static_cast<scenario_configuration>(packed & 15);
}

} // namespace darker::resources
