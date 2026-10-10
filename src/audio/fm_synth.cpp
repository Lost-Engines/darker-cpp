#include "audio/fm_synth.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <dbopl.h>
#include <opl3.h>

namespace darker::audio {

struct fm_synth::implementation {
  std::unique_ptr<opl3_chip> chip;
  std::unique_ptr<DBOPL::Chip> dosbox;
  unsigned int rate{0};
  uint64_t phase{0};
  float previous{0}, current{0};
  bool primed{false};

  float sample() noexcept {
    /// DBOPL synthesises at the reference DOSBox OPL rate, independently of the host device
    Bit32s value{0};
    dosbox->GenerateBlock2(1, &value);
    return static_cast<float>(std::clamp(value, -32768, 32767)) / 32768.0f;
  }
};

fm_synth::fm_synth(unsigned int const sample_rate, fm_backend const backend) : state{std::make_unique<implementation>()} {
  /// Select chip emulation without changing the game's patches, levels or register driver
  if(sample_rate == 0) throw std::invalid_argument{"FM output sample rate must be positive"};
  state->rate = sample_rate;
  if(backend == fm_backend::dosbox) {
    static std::once_flag tables;
    std::call_once(tables, DBOPL::InitTables);
    state->dosbox = std::make_unique<DBOPL::Chip>();
    state->dosbox->Setup(44100);
  } else {
    state->chip = std::make_unique<opl3_chip>();
    OPL3_Reset(state->chip.get(), sample_rate);
  }
  write(fm_write{1, 32});
  write(fm_write{8, 0});
  write(fm_write{0xbd, 0});
}

fm_synth::~fm_synth() = default;

void fm_synth::write(fm_write const command) noexcept {
  /// DOSBox writes immediately; Nuked retains its hardware register-write delay
  if(state->dosbox) state->dosbox->WriteReg(command.address, command.value);
  else OPL3_WriteRegBuffered(state->chip.get(), command.address, command.value);
}

void fm_synth::write(fm_program const &program) noexcept {
  /// Deliver register writes in driver order using the selected core's timing
  for(auto const command :
  program.view()) write(command);
}

void fm_synth::render(std::span<float> const stereo) noexcept {
  /// Convert the chip's stereo signed PCM directly, without normalisation or a DC filter
  for(size_t i{0}; i + 1 < stereo.size(); i += 2) {
    if(state->dosbox) {
      if(state->rate == 44100) {
        stereo[i] = stereo[i + 1] = state->sample();
        continue;
      }
      if(!state->primed) {
        state->current = state->sample();
        state->previous = state->current;
        state->primed = true;
      }
      while(state->phase >= state->rate) {
        state->phase -= state->rate;
        state->previous = state->current;
        state->current = state->sample();
      }
      // causal linear conversion preserves fractional phase across callback buffers
      float const fraction{static_cast<float>(state->phase) / static_cast<float>(state->rate)};
      auto const value{state->previous + (state->current - state->previous) * fraction};
      stereo[i] = value;
      stereo[i + 1] = value;
      state->phase += 44100;
      continue;
    }
    std::array<int16_t, 2> sample{};
    OPL3_GenerateResampled(state->chip.get(), sample.data());
    stereo[i] = static_cast<float>(sample[0]) / 32768.0f;
    stereo[i + 1] = static_cast<float>(sample[1]) / 32768.0f;
  }
}

} // namespace darker::audio
