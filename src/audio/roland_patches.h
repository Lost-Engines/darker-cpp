#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace darker::audio {

using sysex_messages = std::vector<std::vector<uint8_t>>;
/// Read complete Roland DT1 messages from a single-track SysEx setup MIDI file
auto roland_setup_messages(std::span<std::byte const> file)->sysex_messages;

/// Reconstruct the LAPC-I driver's device-initialisation SysEx directly from its compressed timbre and patch tables
auto lapc_initialisation(std::span<std::byte const> driver)->sysex_messages;

} // namespace darker::audio
