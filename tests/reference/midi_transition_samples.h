#pragma once
#include <array>
#include <cstdint>
namespace darker::test_reference {
struct midi_transition_sample { unsigned int variant; bool pause; unsigned int events; uint64_t fingerprint; };
inline constexpr auto midi_transition_samples = std::to_array<midi_transition_sample>({
  {1, false, 1225, 0x29a96d66d58803d8ULL},
  {1, true, 1032, 0xa126214b8da2402aULL},
  {2, false, 1253, 0xf9732dcd3c01e65fULL},
  {2, true, 1059, 0xa825bd382121f09cULL},
  {3, false, 1248, 0xc62ac65574d357aeULL},
  {3, true, 1055, 0x0e745a602a8573ceULL},
  {4, false, 1322, 0x36343796c472f437ULL},
  {4, true, 1128, 0xdbba5682c1bcd300ULL},
});
}
