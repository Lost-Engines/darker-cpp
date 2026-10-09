#pragma once
#include <filesystem>
#include <memory>
#include <span>
#include "audio/midi_synth.h"
#include "audio/roland_patches.h"

namespace darker::audio {

class roland_synth final : public midi_synth {
private:
  struct implementation;
  std::unique_ptr<implementation> state;

public:
  roland_synth(std::filesystem::path const &rom_directory, unsigned int sample_rate, sysex_messages const &initialisation, std::filesystem::path const &percussion_font = {}, std::filesystem::path const &percussion_bank = {});
  ~roland_synth() override;
  void reset() noexcept override;
  void send(midi_message message) noexcept override;
  void render(std::span<float> stereo) noexcept override;
};

} // namespace darker::audio
