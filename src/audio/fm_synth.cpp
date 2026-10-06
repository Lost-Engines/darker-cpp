#include "audio/fm_synth.h"
#include <array>
#include <cstdint>
#include <opl3.h>

namespace darker::audio {

struct fm_synth::implementation {
  opl3_chip chip{};
};

fm_synth::fm_synth(unsigned int const sample_rate) : state{std::make_unique<implementation>()} {
  /// Use the same OPL2-compatible chip setup as the independently reconstructed sound auditions
  OPL3_Reset(&state->chip, sample_rate);
  OPL3_WriteRegBuffered(&state->chip, 1, 32);
  OPL3_WriteRegBuffered(&state->chip, 8, 0);
  OPL3_WriteRegBuffered(&state->chip, 0xbd, 0);
}

fm_synth::~fm_synth() = default;

void fm_synth::write(fm_write const command) noexcept {
  /// Music and effects share the same buffered OPL register interface
  OPL3_WriteRegBuffered(&state->chip,command.address,command.value);
}

void fm_synth::write(fm_program const &program) noexcept {
  /// Deliver register writes in driver order with the emulator's hardware write delay
  for(auto const command : program.view()) write(command);
}

void fm_synth::render(std::span<float> const stereo) noexcept {
  /// Convert the chip's stereo signed PCM directly, without normalisation or a DC filter
  for(std::size_t i{0}; i + 1 < stereo.size(); i += 2) {
    std::array<std::int16_t, 2> sample{};
    OPL3_GenerateResampled(&state->chip, sample.data());
    stereo[i] = static_cast<float>(sample[0]) / 32768.0f;
    stereo[i + 1] = static_cast<float>(sample[1]) / 32768.0f;
  }
}

} // namespace darker::audio
