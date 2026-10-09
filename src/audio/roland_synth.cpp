#include "audio/roland_synth.h"
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <vector>
#define MT32EMU_API_TYPE 1
#include <mt32emu/mt32emu.h>

namespace darker::audio {

struct roland_synth::implementation {
  mt32emu_context context{mt32emu_create_context({}, nullptr)};
  ~implementation() {
    mt32emu_free_context(context);
  }
};

roland_synth::roland_synth(std::filesystem::path const &rom_directory, unsigned int const sample_rate,
  sysex_messages const &initialisation) : state{std::make_unique<implementation>()} {
  if(!state->context) throw std::runtime_error{"Cannot create Munt synthesiser"};
  if(!std::filesystem::is_directory(rom_directory)) throw std::invalid_argument{"Roland ROM directory does not exist: " + rom_directory.string()};
  std::vector<std::filesystem::path> files;
  for(auto const &entry : std::filesystem::directory_iterator{rom_directory}) {
    if(entry.is_regular_file() && entry.file_size() <= 2 * 1024 * 1024) files.push_back(entry.path());
  }
  std::ranges::sort(files);
  bool found{false};
  // Prefer LAPC-I-compatible CM-32L ROMs, then new and old MT-32 machines; recognise contents, not filenames
  for(char const *machine : {"cm32l_1_02", "cm32l_1_00", "mt32_2_07", "mt32_2_06", "mt32_2_04", "mt32_2_03",
    "mt32_1_07", "mt32_1_06", "mt32_1_05", "mt32_1_04"}) {
    mt32emu_free_context(state->context);
    state->context = mt32emu_create_context({}, nullptr);
    if(!state->context) throw std::runtime_error{"Cannot create Munt ROM identification context"};
    for(auto const &file : files) mt32emu_add_machine_rom_file(state->context, machine, file.c_str());
    mt32emu_rom_info info{};
    mt32emu_get_rom_info(state->context, &info);
    if(info.control_rom_id && info.pcm_rom_id) { found = true; break; }
  }
  if(!found) throw std::runtime_error{"No compatible Roland control/PCM ROM pair found in " + rom_directory.string()};
  mt32emu_set_stereo_output_samplerate(state->context, sample_rate);
  mt32emu_set_analog_output_mode(state->context, MT32EMU_AOM_ACCURATE);
  if(mt32emu_open_synth(state->context) != MT32EMU_RC_OK) throw std::runtime_error{"Munt could not open the Roland ROM pair"};
  mt32emu_set_midi_event_queue_size(state->context, 4096);
  mt32emu_set_midi_delay_mode(state->context, MT32EMU_MDM_DELAY_SHORT_MESSAGES_ONLY);
  // Hardware initialisation finishes before music starts; immediate uploads retain timbres throughout song changes
  for(auto const &message : initialisation) mt32emu_play_sysex_now(state->context, message.data(), static_cast<uint32_t>(message.size()));
}

roland_synth::~roland_synth() = default;

void roland_synth::reset() noexcept {
  /// Stop voices without a device reset, which would erase the game's uploaded instruments
  mt32emu_flush_midi_queue(state->context);
  for(uint32_t channel{0}; channel < 16; ++channel) {
    mt32emu_play_msg_now(state->context, 0xb0 | channel | (64 << 8));
    mt32emu_play_msg_now(state->context, 0xb0 | channel | (120 << 8));
    mt32emu_play_msg_now(state->context, 0xb0 | channel | (123 << 8));
  }
}

void roland_synth::send(midi_message const message) noexcept {
  uint32_t const packed{message.status | (static_cast<uint32_t>(message.first) << 8) | (static_cast<uint32_t>(message.second) << 16)};
  if(mt32emu_play_msg(state->context, packed) != MT32EMU_RC_OK) std::terminate();
}

void roland_synth::render(std::span<float> const stereo) noexcept {
  mt32emu_render_float(state->context, stereo.data(), static_cast<uint32_t>(stereo.size() / 2));
}

} // namespace darker::audio
