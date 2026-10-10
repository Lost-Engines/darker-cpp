#pragma once
#include <filesystem>
#include <memory>
#include "audio/midi_synth.h"

namespace darker::audio {

class gus_synth final : public midi_synth {
private:
  struct implementation;
  std::unique_ptr<implementation> state;

public:
  gus_synth(std::filesystem::path const &patch_directory, unsigned int sample_rate, unsigned int ram_kib = 1024);
  ~gus_synth() override;
  void reset() noexcept override;
  void send(midi_message message) noexcept override;
  void render(std::span<float> stereo) noexcept override;
};

} // namespace darker::audio
