#pragma once
#include <filesystem>
#include <memory>
#include <span>
#include "audio/midi_music.h"

namespace darker::audio {

class soundfont {
private:
  struct implementation;
  std::unique_ptr<implementation> state;

public:
  soundfont(std::filesystem::path const &path, unsigned int sample_rate);
  ~soundfont();
  void reset() noexcept;
  void send(midi_message message) noexcept;
  void render(std::span<float> stereo) noexcept;
};

} // namespace darker::audio
