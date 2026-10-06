#include "audio/fm_stream.h"
#include <boost/lockfree/spsc_queue.hpp>
#include "audio/fm_driver.h"
#include "audio/fm_synth.h"

namespace darker::audio {

struct fm_stream::implementation {
  boost::lockfree::spsc_queue<fm_frame, boost::lockfree::capacity<128>> frames;
  fm_frame previous{};
  fm_driver driver;
  fm_synth synth;

  explicit implementation(unsigned int const sample_rate) : synth{sample_rate} {}
};

fm_stream::fm_stream(unsigned int const sample_rate) : state{std::make_unique<implementation>(sample_rate)} {
  /// Construct chip and queue before the device starts consuming audio
}

fm_stream::~fm_stream() = default;

bool fm_stream::publish(fm_frame const &frame) noexcept {
  /// The simulation is the sole producer; immutable snapshots cross into the audio callback
  for(auto const &note : frame) if(note.patch >= 39) return false;
  return state->frames.push(frame);
}

void fm_stream::render(std::span<float> const stereo) noexcept {
  /// Keep synthesis, driver caches and all OPL writes on the audio thread without locks or allocations
  fm_frame frame;
  while(state->frames.pop(frame)) {
    for(std::uint8_t channel{0}; channel < frame.size(); ++channel) {
      auto const &note{frame[channel]};
      if(note.active) {
        state->synth.write(state->driver.program(channel, note.patch, note.pitch, note.level, true,
          note.generation != state->previous[channel].generation));
      } else if(state->previous[channel].active) {
        state->synth.write(state->driver.stop(channel));
      }
    }
    state->previous = frame;
  }
  state->synth.render(stereo);
}

} // namespace darker::audio
