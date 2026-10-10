#pragma once

#include <cstdint>

namespace darker::game {

// these aliases document native units; arithmetic deliberately retains integer wrapping
using clock_tick = uint16_t;                                                   // low word of the running timer, including wrapping deadlines
using game_duration = uint16_t;                                                // native timer ticks, not milliseconds or frame counts
using campaign_clock = uint32_t;                                               // extended timer used by mission scripts

} // namespace darker::game
