#include "audio/soundfont.h"
#include <stdexcept>
#include <tsf.h>

namespace darker::audio {

struct soundfont::implementation {
  std::unique_ptr<tsf, decltype(&tsf_close)> synth{nullptr, tsf_close};
};

soundfont::soundfont(std::filesystem::path const &path, unsigned int const sample_rate) : state{std::make_unique<implementation>()} {
  state->synth.reset(tsf_load_filename(path.string().c_str()));
  if(!state->synth) throw std::runtime_error{"Cannot load SoundFont: " + path.string()};
  tsf_set_output(state->synth.get(), TSF_STEREO_INTERLEAVED, static_cast<int>(sample_rate), -6.0f);
  if(!tsf_set_max_voices(state->synth.get(), 256)) throw std::runtime_error{"Cannot allocate SoundFont voices"};
  for(int channel{0}; channel < 16; ++channel) {
    if(!tsf_channel_set_presetnumber(state->synth.get(), channel, 0, channel == 9)) throw std::runtime_error{"SoundFont requires melodic and percussion presets"};
  }
}

soundfont::~soundfont() = default;

void soundfont::reset() noexcept {
  /// Keep channels allocated; hard-stop voices before a new presentation or flight sound takes over
  auto *synth{state->synth.get()};
  for(int channel{0}; channel < 16; ++channel) {
    tsf_channel_sounds_off_all(synth, channel);
    tsf_channel_midi_control(synth, channel, 121, 0);
    tsf_channel_set_sustain(synth, channel, 0);
    tsf_channel_set_pitchwheel(synth, channel, 8192);
    tsf_channel_set_bank(synth, channel, 0);
    tsf_channel_set_presetnumber(synth, channel, 0, channel == 9);
  }
}

void soundfont::send(midi_message const message) noexcept {
  auto *synth{state->synth.get()};
  int const channel{message.status & 15};
  switch(message.status & 0xf0) {
  case 0x80: tsf_channel_note_off(synth, channel, message.first); break;
  case 0x90: tsf_channel_note_on(synth, channel, message.first, message.second / 127.0f); break;
  case 0xb0: tsf_channel_midi_control(synth, channel, message.first, message.second); break;
  case 0xc0: tsf_channel_set_presetnumber(synth, channel, message.first, channel == 9); break;
  case 0xe0: tsf_channel_set_pitchwheel(synth, channel, message.first | (message.second << 7)); break;
  default: break;
  }
}

void soundfont::render(std::span<float> const stereo) noexcept {
  tsf_render_float(state->synth.get(), stereo.data(), static_cast<int>(stereo.size() / 2), 0);
}

} // namespace darker::audio
