#pragma once
#include <array>
#include <cstdint>

namespace darker::test_reference {
struct city_persistence_sample { unsigned int bank, map, stage, bytes; uint64_t packed, restored; };
inline constexpr auto city_persistence_samples = std::to_array<city_persistence_sample>({
  {30, 68, 1, 953, 0x3336d79c8a2035afULL, 0xb960beee52c97998ULL},
  {30, 68, 2, 953, 0x3336d79c8a2035afULL, 0x71390e6db32714d4ULL},
  {31, 69, 1, 468, 0xa98a497b7c373ae8ULL, 0x9c1bda7f8c872325ULL},
  {31, 69, 2, 468, 0xa98a497b7c373ae8ULL, 0xed1e93f465bf8ec5ULL},
});
} // namespace darker::test_reference
