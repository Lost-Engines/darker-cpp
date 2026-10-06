#pragma once
#include <array>
#include <cstdint>

namespace darker::test_reference {
struct music_sample { unsigned int resource, ticks, writes; uint64_t fingerprint; };
inline constexpr auto music_samples = std::to_array<music_sample>({
  {38, 16384, 28347, 0xd042892c472efa6eULL},
  {43, 16384, 26881, 0xebe4c670c9833d6eULL},
  {48, 16384, 49965, 0x0927997b65329bbcULL},
  {53, 16384, 17978, 0xe92baaf001db4275ULL},
  {58, 16384, 42224, 0x9669cc6e8292e555ULL},
  {63, 16384, 25688, 0xf4bc3d50e6d416e3ULL},
});
struct music_transition_sample { unsigned int source, target; bool pause; unsigned int writes; uint64_t fingerprint; };
inline constexpr auto music_transition_samples = std::to_array<music_transition_sample>({
  {0, 1, false, 2276, 0x19ad23e8a5878131ULL},
  {0, 1, true, 1909, 0xdefff5f91b10e314ULL},
  {1, 2, false, 2714, 0xd0c2a0b95f128575ULL},
  {1, 2, true, 1987, 0xb188735ad714fa11ULL},
  {2, 3, false, 1933, 0x67efd4e8bac71b12ULL},
  {2, 3, true, 1732, 0x71bf63c753034f17ULL},
  {3, 4, false, 2008, 0xdec1df3f4432a245ULL},
  {3, 4, true, 1343, 0x6d3ad18aee37fcddULL},
  {4, 5, false, 1948, 0x0c47708ce3a79083ULL},
  {4, 5, true, 1210, 0xbc250f5bd51eec92ULL},
  {5, 0, false, 1901, 0xea31ad8760b73a86ULL},
  {5, 0, true, 1531, 0x2d1eaccb2f3977fdULL},
});
} // namespace darker::test_reference
