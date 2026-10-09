#pragma once
#include <array>
#include <cstdint>
namespace darker::test_reference {
struct midi_music_sample { unsigned int variant, group, ticks, events; uint64_t fingerprint; };
inline constexpr auto midi_music_samples = std::to_array<midi_music_sample>({
  {1, 0, 16384, 20185, 0xeb655e69432bea32ULL},
  {1, 1, 16384, 16596, 0xf99b952ff28abd80ULL},
  {1, 2, 16384, 38859, 0xcbdab0a5ec98b0a2ULL},
  {1, 3, 16384, 22605, 0xa6191d1a8bd49c60ULL},
  {1, 4, 16384, 36759, 0x1914beeb861266c6ULL},
  {1, 5, 16384, 30085, 0xee22339f43b65a96ULL},
  {2, 0, 16384, 20189, 0x367c1029ef79ca91ULL},
  {2, 1, 16384, 18298, 0xfa0d8eb6e6431fcaULL},
  {2, 2, 16384, 38831, 0xfcd017d37720bd60ULL},
  {2, 3, 16384, 22588, 0xc8b513f4a42e8aabULL},
  {2, 4, 16384, 36730, 0xd9646691a4a70258ULL},
  {2, 5, 16384, 18026, 0x043236a22891a809ULL},
  {3, 0, 16384, 21694, 0x259c197554e37aa3ULL},
  {3, 1, 16384, 18282, 0xbdb5722ef60bc5faULL},
  {3, 2, 16384, 38831, 0x67285a4f1ce04e8cULL},
  {3, 3, 16384, 22575, 0xf48c0389db248c4eULL},
  {3, 4, 16384, 36729, 0x735d7c3406a2dedbULL},
  {3, 5, 16384, 30721, 0x32b68d0e256ae781ULL},
  {4, 0, 16384, 21770, 0x9641b3c2e9dd30f4ULL},
  {4, 1, 16384, 18356, 0x5a1775ace7509766ULL},
  {4, 2, 16384, 38859, 0xd9c38b2b6bbf2dd6ULL},
  {4, 3, 16384, 22605, 0x35c23a318d8ba994ULL},
  {4, 4, 16384, 36719, 0x33adbb6a7cf27cb8ULL},
  {4, 5, 16384, 30949, 0x6221353060e077faULL},
});
}
