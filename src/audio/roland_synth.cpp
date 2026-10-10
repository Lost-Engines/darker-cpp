#include "audio/roland_synth.h"
#include <algorithm>
#include <array>
#include <exception>
#include <stdexcept>
#include <vector>
#define MT32EMU_API_TYPE 1
#include <mt32emu/mt32emu.h>
#include "audio/soundfont.h"

namespace darker::audio {

struct roland_synth::implementation {
  mt32emu_context context{mt32emu_create_context({}, nullptr)};
  std::unique_ptr<midi_synth> percussion;
  std::array<bool,128> unmapped{};
  ~implementation() {
    mt32emu_free_context(context);
  }
};

roland_synth::roland_synth(std::filesystem::path const &rom_directory, unsigned int const sample_rate,
  sysex_messages const &initialisation, std::filesystem::path const &percussion_font, std::filesystem::path const &percussion_bank) : state{std::make_unique<implementation>()} {
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
    for(auto const &file : files) mt32emu_add_machine_rom_file(state->context, machine, file.string().c_str());
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
  if(!percussion_font.empty() && !percussion_bank.empty()) throw std::invalid_argument{"Choose one Roland percussion fallback"};
  if(!percussion_bank.empty()) {
    // A separate device prevents the GM bank's patch/channel/system changes from overwriting Darker's setup.
    state->percussion = std::make_unique<roland_synth>(rom_directory, sample_rate, load_roland_setup(percussion_bank));
  } else if(!percussion_font.empty()) state->percussion = std::make_unique<soundfont>(percussion_font, sample_rate);
  if(state->percussion) {
    // Roland address 03 01 10 uses seven-bit address digits; each rhythm entry has four bytes.
    // Only supplement explicitly OFF keys within the General MIDI percussion range.
    for(unsigned int key{35}; key <= 81; ++key) {
      uint8_t timbre{0};
      mt32emu_read_memory(state->context, (3u << 14) + (1u << 7) + 0x10 + (key - 24) * 4, 1, &timbre);
      state->unmapped[key] = timbre == 127;
    }
  }
}

roland_synth::~roland_synth() = default;

void roland_synth::reset() noexcept {
  /// Stop voices without a device reset, which would erase the game's uploaded instruments
  if(state->percussion) state->percussion->reset();
  mt32emu_flush_midi_queue(state->context);
  for(uint32_t channel{0}; channel < 16; ++channel) {
    mt32emu_play_msg_now(state->context, 0xb0 | channel | (64 << 8));
    mt32emu_play_msg_now(state->context, 0xb0 | channel | (120 << 8));
    mt32emu_play_msg_now(state->context, 0xb0 | channel | (123 << 8));
  }
}

void roland_synth::send(midi_message const message) noexcept {
  if(state->percussion && (message.status & 15) == 9) {
    auto const command{message.status & 0xf0};
    if(((command == 0x80 || command == 0x90) && message.first < state->unmapped.size() && state->unmapped[message.first])
      || command == 0xb0 || command == 0xe0) state->percussion->send(message);
  }
  // Still send every event to Munt, preserving its unmapped-key diagnostics.
  uint32_t const packed{message.status | (static_cast<uint32_t>(message.first) << 8) | (static_cast<uint32_t>(message.second) << 16)};
  if(mt32emu_play_msg(state->context, packed) != MT32EMU_RC_OK) std::terminate();
}

void roland_synth::render(std::span<float> const stereo) noexcept {
  mt32emu_render_float(state->context, stereo.data(), static_cast<uint32_t>(stereo.size() / 2));
  if(state->percussion) {
    std::array<float,1024> percussion{};
    for(size_t offset{0}; offset + 1 < stereo.size();) {
      auto const count{std::min((stereo.size() - offset) & ~size_t{1}, percussion.size())};
      state->percussion->render(std::span{percussion}.first(count));
      for(size_t i{0}; i < count; ++i) stereo[offset + i] += percussion[i];
      offset += count;
    }
  }
}

} // namespace darker::audio
