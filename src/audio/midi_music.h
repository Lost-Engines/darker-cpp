#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace darker::audio {

enum class music_variant : uint8_t { soundblaster, scc1, lapc1, gus, awe32 };
struct midi_message {
  uint8_t status, first, second{0};
  auto operator==(midi_message const &) const->bool = default;
};
using midi_sink = std::function<void(midi_message)>;

class midi_music {
private:
  struct track {
    size_t cursor{0}, loop{0};
    int64_t delay{0};
    uint8_t channel{0}, note{0};
    bool active{false};
  };
  music_variant variant;
  std::vector<std::byte> sequence;
  std::array<track,16> tracks{};
  uint32_t clock_fraction{0x10001}, increment{0};
  uint8_t division{0}, tempo{0}, track_count{0};
  auto byte(track &part)->uint8_t;
  auto delta(track &part)->uint32_t;
  void timing();
  void event(track &part, midi_sink const &sink);

public:
  explicit midi_music(music_variant variant);
  void start(std::span<std::byte const> song, midi_sink const &sink);
  void advance(midi_sink const &sink);
  void stop(midi_sink const &sink);
};

} // namespace darker::audio
