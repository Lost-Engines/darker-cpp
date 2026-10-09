#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace darker::audio {

/// Reconstruct the LAPC-I driver's device-initialisation SysEx directly from its compressed timbre and patch tables
using sysex_messages = std::vector<std::vector<uint8_t>>;
auto lapc_initialisation(std::span<std::byte const> driver)->sysex_messages;

} // namespace darker::audio
