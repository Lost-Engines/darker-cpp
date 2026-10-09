#pragma once
#include <filesystem>
#include <memory>
#include "audio/midi_synth.h"

namespace darker::audio {

class awe32_synth final : public midi_synth {
private:
  struct implementation;
  std::unique_ptr<implementation> state;

public:
  awe32_synth(std::filesystem::path const &rom, std::span<std::byte const> driver, unsigned int sample_rate);
  ~awe32_synth() override;
  void reset() noexcept override;
  void send(midi_message message) noexcept override;
  void render(std::span<float> stereo) noexcept override;
};

} // namespace darker::audio
