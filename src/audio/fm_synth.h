#pragma once

#include <memory>
#include <span>
#include "audio/fm_driver.h"

namespace darker::audio {

class fm_synth {
private:
  struct implementation;
  std::unique_ptr<implementation> state;

public:
  explicit fm_synth(unsigned int sample_rate);
  ~fm_synth();
  void write(fm_write command) noexcept;
  void write(fm_program const &program) noexcept;
  void render(std::span<float> stereo) noexcept;
};

} // namespace darker::audio
