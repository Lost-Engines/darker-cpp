#include "game/effect_sound.h"

namespace darker::game {

effect_sound_pool::effect_sound_pool(uint32_t const first_identity) noexcept : first_identity{first_identity} {
  /// Reverse the initial free stack so physical identities are allocated in ascending order
  for(unsigned int i{0}; i < capacity; ++i) free_slots[i] = static_cast<uint8_t>(capacity - i - 1);
}

void effect_sound_pool::append(effect_sound sound) {
  /// 1CBF takes a free record or recycles the oldest active record, preserving its physical identity
  active.reserve(capacity);                                                    // allocate before mutating slot bookkeeping; subsequent insertion cannot allocate
  uint8_t slot;
  if(free_count != 0) slot = free_slots[--free_count];
  else {
    slot = static_cast<uint8_t>(active.front().identity - first_identity);
    active.erase(active.begin());
  }
  sound.identity = first_identity + slot;
  sound.generation = ++generations[slot];
  active.push_back(sound);
}

} // namespace darker::game
