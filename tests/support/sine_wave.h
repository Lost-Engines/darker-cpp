#pragma once

#include <span>

namespace framework::audio {

class sine_wave {
private:
  double phase{0.0};
  double phase_step;
  float amplitude;
  unsigned int ramp_frames;
  unsigned int elapsed_frames{0};

public:
  sine_wave(unsigned int sample_rate, double frequency, float amplitude);
  void fill_stereo(std::span<float> samples) noexcept;
};

} // namespace framework::audio
