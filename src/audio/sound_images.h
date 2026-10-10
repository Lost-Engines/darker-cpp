#pragma once

#include <array>
#include <functional>
#include <span>
#include <vector>
#include "audio/fm_driver.h"

namespace darker::audio {

using fm_sink = std::function<void(fm_write)>;

class sound_images {
private:
  struct track {
    size_t cursor{0}, loop{0};
    int64_t delay{0};
    uint16_t instrument{0}, operators{0};
    uint8_t channel{0}, note{0}, key{0}, program{255}, volume{127}, velocity{0}, percussion{0}, bend_range{2}, bend{64};
    bool active{false};
  };
  std::vector<std::byte> driver, sequence;
  std::array<track, 16> tracks{};
  std::array<uint8_t, 256> registers{};
  uint32_t clock_fraction{0x10001}, increment{0};
  uint8_t division{0}, tempo{0};
  uint8_t read(size_t offset) const;
  uint16_t word(size_t offset) const;
  uint8_t byte(track &part) const;
  uint32_t delta(track &part) const;
  void timing();
  void write(uint8_t address, uint8_t value, fm_sink const &sink);
  void instrument(track &part, fm_sink const &sink);
  void pitch(track &part, uint8_t note, fm_sink const &sink);
  void level(track const &part, fm_sink const &sink);
  void release(track &part, fm_sink const &sink);
  void event(track &part, fm_sink const &sink);

public:
  explicit sound_images(std::span<std::byte const> driver);
  void start(std::span<std::byte const> song, fm_sink const &sink);
  void advance(fm_sink const &sink);
  void stop(fm_sink const &sink);
};

} // namespace darker::audio
