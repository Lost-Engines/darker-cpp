#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>
#include "game/time.h"
#include "maths/world_coordinates.h"

namespace darker::game {

struct effect_sound_definition {
  static uint8_t constexpr spatial_stereo_flag{1};                             // apply left/right attenuation from the source bearing
  static uint8_t constexpr submitted_flags{0x28};                              // suppress fresh admission after the source has had its first voice attempt
  static uint8_t constexpr retire_without_voice_flag{0x20};
  game_duration duration;
  uint16_t pitch;
  uint16_t level;
  uint8_t patch;
  uint8_t flags;
};

struct effect_sound {
  maths::world_position position{};
  effect_sound_definition definition{};
  clock_tick deadline{0};
  uint32_t identity{0};
  uint16_t generation{0};
};

class effect_sound_pool {
public:
  static unsigned int constexpr capacity{16};

  explicit effect_sound_pool(uint32_t first_identity) noexcept;
  void append(effect_sound sound);
  std::span<effect_sound const> records() const noexcept { return active; }

  template<typename Predicate> void retire(Predicate const &expired) {
    /// Recycle in newest-first order, preserving the native free-list allocation sequence
    for(unsigned int i{static_cast<unsigned int>(active.size())}; i != 0; --i) {
      auto const index{i - 1};
      if(!expired(std::as_const(active[index]))) continue;
      free_slots[free_count++] = static_cast<uint8_t>(active[index].identity - first_identity);
      active.erase(active.begin() + static_cast<ptrdiff_t>(index));
    }
  }

  template<typename Visitor> void submit(Visitor const &visit) {
    /// Submit newest-first, then mark each source for retirement if its allocated voice is subsequently lost
    for(auto i{active.rbegin()}; i != active.rend(); ++i) {
      visit(std::as_const(*i));
      i->definition.flags |= effect_sound_definition::submitted_flags;
    }
  }

private:
  uint32_t first_identity;
  std::vector<effect_sound> active;
  std::array<uint8_t, capacity> free_slots{};
  std::array<uint16_t, capacity> generations{};
  unsigned int free_count{capacity};
};

} // namespace darker::game
