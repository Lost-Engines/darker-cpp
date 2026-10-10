#include "sine_wave.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace framework::audio {

sine_wave::sine_wave(unsigned int const sample_rate, double const frequency, float const amplitude) :
  phase_step{0.0},
  amplitude{amplitude},
  ramp_frames{std::max(1u, sample_rate / 100u)} {
  /// Create a bounded oscillator with a ten-millisecond startup ramp
  if(sample_rate == 0 || !std::isfinite(frequency) || frequency <= 0.0 || frequency >= sample_rate / 2.0) {
    throw std::invalid_argument{"sine frequency must be positive and below the Nyquist frequency"};
  }
  if(!std::isfinite(amplitude) || amplitude < 0.0f || amplitude > 1.0f) {
    throw std::invalid_argument{"sine amplitude must be between zero and one"};
  }
  phase_step = 2.0 * std::numbers::pi * frequency / sample_rate;
}

void sine_wave::fill_stereo(std::span<float> const samples) noexcept {
  /// Generate identical left/right PCM samples without allocating or restarting phase between buffers
  assert(samples.size() % 2 == 0 && "sine_wave requires complete stereo frames");
  for(size_t frame{0}; frame != samples.size() / 2; ++frame) {
    float const gain{static_cast<float>(elapsed_frames) / static_cast<float>(ramp_frames)};
    float const sample{amplitude * gain * static_cast<float>(std::sin(phase))};
    samples[frame * 2] = sample;
    samples[frame * 2 + 1] = sample;
    elapsed_frames = std::min(elapsed_frames + 1, ramp_frames);
    phase += phase_step;
    if(phase >= 2.0 * std::numbers::pi) phase -= 2.0 * std::numbers::pi;
  }
}

} // namespace framework::audio
