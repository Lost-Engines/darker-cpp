#pragma once
#include <span>
#include "audio/midi_music.h"

namespace darker::audio {

class midi_synth {
public:
  virtual ~midi_synth() = default;
  virtual void reset() noexcept = 0;
  virtual void send(midi_message message) noexcept = 0;
  virtual void render(std::span<float> stereo) noexcept = 0;
};

} // namespace darker::audio
