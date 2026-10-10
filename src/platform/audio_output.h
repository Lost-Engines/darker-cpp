#pragma once

#include <memory>
#include <span>

namespace framework::platform {

using pcm_callback = void (*)(void*, std::span<float>) noexcept;

class audio_output {
private:
  struct implementation;
  std::unique_ptr<implementation> state;

public:
  static unsigned int constexpr sample_rate{48'000};
  static unsigned int constexpr channels{2};

  audio_output(pcm_callback callback, void *userdata);
  ~audio_output();
  audio_output(audio_output const&) = delete;
  audio_output &operator=(audio_output const&) = delete;
  audio_output(audio_output &&) = delete;
  audio_output &operator=(audio_output &&) = delete;
};

} // namespace framework::platform
