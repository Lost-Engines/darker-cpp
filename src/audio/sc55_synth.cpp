#include "audio/sc55_synth.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <diagnostics.h>
#include <emu.h>
#include <file_hashing.h>
#include <standard_romsets.h>

namespace darker::audio {

struct sc55_synth::implementation {
  RomsetInfo roms;
  Emulator synth;
  unsigned int rate{0}, native_rate{0};
  uint64_t phase{0};
  AudioFrame<float> previous{}, current{}, received{};
  bool ready{false}, primed{false};

  auto sample() noexcept->AudioFrame<float> {
    ready = false;
    while(!ready) synth.Step();
    return received;
  }
};

sc55_synth::sc55_synth(std::filesystem::path const &rom_directory, unsigned int const sample_rate, sound_canvas_model const model)
  : state{std::make_unique<implementation>()} {
  if(sample_rate == 0) throw std::invalid_argument{"Sound Canvas output sample rate must be positive"};
  static std::once_flag diagnostics;
  std::call_once(diagnostics, [] {
    Diag_SetCallback([](Diag_Category category, std::string_view message) {
      if(category != Diag_Category::Debug) Diag_DefaultCallback(category, message);
    });
  });
  bool const scc1a{model == sound_canvas_model::scc1a};
  std::string const name{scc1a ? "SCC-1A v1.30" : "SC-55 v1.21"};
  HashedFileRegistry files;
  RomsetRegistry definitions;
  for(auto const &definition : GetStandardRomsetDefinitions()) definitions.AddRomset(definition);
  RomLocationSet locations;
  locations.fill(true);
  if(!HashDirectoryFiles(rom_directory, HashDirectoryKind::TopLevel, files,
    [](std::filesystem::directory_entry const &entry) { return entry.path().extension() == ".bin" && entry.file_size() <= 1048576; })
    || !GetRomsetInfo(definitions, scc1a ? "cm300-v1.30" : "mk1-v1.21", files, locations, state->roms))
    throw std::runtime_error{name + " ROMs missing or unrecognised in " + rom_directory.string() + "; supply a complete matching ROM set"};
  if(!LoadRomset(state->roms, nullptr) || !state->synth.Init({}) || !state->synth.LoadRoms(scc1a ? Romset::CM300 : Romset::MK1, state->roms))
    throw std::runtime_error{"Cannot initialise " + name + " emulation"};
  state->rate = sample_rate;
  state->synth.Reset();
  // use the chip's oversampled output and preserve its clock independently of host buffers
  state->synth.GetPCM().enable_oversampling = true;
  state->native_rate = PCM_GetOutputFrequency(state->synth.GetPCM());
  state->synth.SetSampleCallback([](void *context, AudioFrame<int32_t> const &frame) {
    auto &target{*static_cast<implementation *>(context)};
    Normalize(frame, target.received);
    target.ready = true;
  }, state.get());
  // run firmware startup before any game events arrive, outside the audio callback
  for(unsigned int i{0}; i < state->native_rate * 3; ++i) state->sample();
}

sc55_synth::~sc55_synth() = default;

void sc55_synth::send(midi_message const message) noexcept {
  std::array<uint8_t,3> const bytes{message.status, message.first, message.second};
  auto const kind{message.status & 0xf0};
  state->synth.PostMIDI(std::span{bytes}.first(kind == 0xc0 || kind == 0xd0 ? 2 : 3));
}

void sc55_synth::reset() noexcept {
  // silence notes without power-cycling firmware or losing effects and device state
  for(uint8_t channel{0}; channel < 16; ++channel) {
    send({static_cast<uint8_t>(0xb0 | channel), 120, 0});
    send({static_cast<uint8_t>(0xb0 | channel), 121, 0});
  }
}

void sc55_synth::render(std::span<float> const stereo) noexcept {
  assert(stereo.size() % 2 == 0);
  for(size_t i{0}; i < stereo.size(); i += 2) {
    if(!state->primed) {
      state->current = state->previous = state->sample();
      state->primed = true;
    }
    while(state->phase >= state->rate) {
      state->phase -= state->rate;
      state->previous = state->current;
      state->current = state->sample();
    }
    float const fraction{static_cast<float>(state->phase) / static_cast<float>(state->rate)};
    stereo[i] = state->previous.left + (state->current.left - state->previous.left) * fraction;
    stereo[i+1] = state->previous.right + (state->current.right - state->previous.right) * fraction;
    state->phase += state->native_rate;
  }
}

} // namespace darker::audio
