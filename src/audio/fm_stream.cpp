#include "audio/fm_stream.h"
#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <utility>
#include <boost/lockfree/spsc_queue.hpp>
#include "audio/fm_driver.h"
#include "audio/fm_synth.h"
#include "audio/sound_images.h"

namespace darker::audio {

struct fm_stream::implementation {
  static constexpr uint64_t pit_frequency{1193180};
  boost::lockfree::spsc_queue<fm_frame, boost::lockfree::capacity<128>> frames;
  fm_frame previous{};
  fm_driver driver;
  fm_synth synth;
  std::unique_ptr<sound_images> music;
  std::array<std::vector<std::byte>,6> songs;
  std::atomic<int> requested{-1};
  int playing{-1};
  uint64_t phase{0}, period;
  fm_sink sink;

  explicit implementation(unsigned int const sample_rate) : synth{sample_rate}, period{static_cast<uint64_t>(sample_rate) * 23860},
    sink{[this](fm_write const command){ synth.write(command); }} {
    /// The game invokes music every ten 2386-cycle PIT interrupts; driver tempo arithmetic separately uses 5D24
  }

  void effects(fm_frame const &frame) {
    /// Preserve queued effect transitions in producer order, including short retriggers
    for(uint8_t channel{0}; channel < frame.size(); ++channel) {
      auto const &note{frame[channel]};
      if(note.active) synth.write(driver.program(channel,note.patch,note.pitch,note.level,true,
        note.generation != previous[channel].generation));
      else if(previous[channel].active) synth.write(driver.stop(channel));
    }
    previous = frame;
  }

  void silence() noexcept {
    /// Relinquish the music channels before reprogramming the same chip for procedural flight effects
    for(uint8_t channel{0}; channel < 9; ++channel) synth.write(fm_write{static_cast<uint8_t>(0xb0 + channel),0});
    for(uint8_t offset{0}; offset < 22; ++offset) synth.write(fm_write{static_cast<uint8_t>(0x40 + offset),63});
    synth.write(fm_write{0xbd,0});
    driver = {};
    previous = {};
  }
};

fm_stream::fm_stream(unsigned int const sample_rate) : state{std::make_unique<implementation>(sample_rate)} {
  /// Construct chip and queue before the device starts consuming audio
}

fm_stream::~fm_stream() = default;

void fm_stream::configure_music(std::span<std::byte const> const driver, std::array<std::vector<std::byte>,6> songs) {
  /// Load and reserve all music storage before starting the audio device
  for(auto const &song : songs) if(song.empty() || song.size() > 65536) throw std::invalid_argument{"Invalid Sound Images music resource size"};
  state->music = std::make_unique<sound_images>(driver);
  state->songs = std::move(songs);
}

void fm_stream::select_music(int const group) noexcept {
  /// Publish a group selector only; the callback owns all sequencing and chip state
  state->requested.store(group >= 0 && group < 6 ? group : -1,std::memory_order_relaxed);
}

bool fm_stream::publish(fm_frame const &frame) noexcept {
  /// The simulation is the sole producer; immutable snapshots cross into the audio callback
  for(auto const &note : frame) if(note.patch >= 39) return false;
  return state->frames.push(frame);
}

void fm_stream::render(std::span<float> const stereo) noexcept {
  /// Render music or effects on the same OPL chip; unexpected failures terminate at the noexcept device boundary
  int const requested{state->requested.load(std::memory_order_relaxed)};
  if(requested != state->playing) {
    if(requested < 0) {
      if(state->music) state->music->stop(state->sink);
      state->silence();
    }
    state->playing = requested;
    state->phase = 0;
    if(requested >= 0 && state->music) state->music->start(state->songs[static_cast<size_t>(requested)],state->sink);
  }
  fm_frame frame;
  while(state->frames.pop(frame)) if(state->playing < 0 || !state->music) state->effects(frame);
  if(state->playing >= 0 && state->music) {
    size_t cursor{0};
    while(cursor + 1 < stereo.size()) {
      auto const frames{std::min<size_t>((stereo.size() - cursor) / 2,static_cast<size_t>((state->period - state->phase + implementation::pit_frequency - 1) / implementation::pit_frequency))};
      state->synth.render(stereo.subspan(cursor,frames * 2));
      cursor += frames * 2;
      state->phase += frames * implementation::pit_frequency;
      if(state->phase >= state->period) {
        state->phase -= state->period;
        state->music->advance(state->sink);
      }
    }
  } else {
    state->synth.render(stereo);
  }
}

} // namespace darker::audio
