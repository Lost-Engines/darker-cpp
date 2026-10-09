#pragma once
#include <filesystem>
#include <memory>
#include "audio/midi_synth.h"

namespace darker::audio {

class sc55_synth final : public midi_synth {
private:
  struct implementation;
  std::unique_ptr<implementation> state;

public:
  sc55_synth(std::filesystem::path const &rom_directory, unsigned int sample_rate);
  ~sc55_synth() override;
  void reset() noexcept override;
  void send(midi_message message) noexcept override;
  void render(std::span<float> stereo) noexcept override;
};

} // namespace darker::audio
