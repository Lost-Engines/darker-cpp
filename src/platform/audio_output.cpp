#include "audio_output.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <miniaudio.h>

namespace framework::platform {

struct audio_output::implementation {
  ma_device device;
  pcm_callback callback;
  void *userdata;

  implementation(pcm_callback const callback, void *const userdata) :
    callback{callback},
    userdata{userdata} {
    /// Open a PCM playback device; the callback and its data must outlive playback
    if(callback == nullptr) throw std::invalid_argument{"audio PCM callback must not be null"};
    auto config{ma_device_config_init(ma_device_type_playback)};
    config.playback.format = ma_format_f32;
    config.playback.channels = channels;
    config.sampleRate = sample_rate;
    config.pUserData = this;
    config.dataCallback = [](ma_device *const device, void *const output, void const*, ma_uint32 const frames){
      auto &self{*static_cast<implementation*>(device->pUserData)};
      self.callback(self.userdata, {static_cast<float*>(output), static_cast<size_t>(frames) * channels});
    };
    auto const result{ma_device_init(nullptr, &config, &device)};
    if(result != MA_SUCCESS) {
      throw std::runtime_error{std::string{"cannot open PCM playback device: "} + ma_result_description(result)};
    }
  }

  ~implementation() {
    /// Stop and join audio processing before the callback state is destroyed
    ma_device_uninit(&device);
  }

  implementation(implementation const&) = delete;
  implementation &operator=(implementation const&) = delete;
  implementation(implementation &&) = delete;
  implementation &operator=(implementation &&) = delete;
};

audio_output::audio_output(pcm_callback const callback, void *const userdata) :
  state{std::make_unique<implementation>(callback, userdata)} {
  /// Start the device only after its stable callback state is fully constructed
  auto const result{ma_device_start(&state->device)};
  if(result != MA_SUCCESS) {
    throw std::runtime_error{std::string{"cannot start PCM playback: "} + ma_result_description(result)};
  }
  std::cout << "PCM output: " << state->device.playback.name << " (48 kHz, stereo float)" << std::endl;
}

audio_output::~audio_output() = default;

} // namespace framework::platform
