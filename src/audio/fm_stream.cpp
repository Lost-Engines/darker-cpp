#include "audio/fm_stream.h"
#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <utility>
#include <boost/lockfree/spsc_queue.hpp>
#include "audio/fm_driver.h"
#include "audio/fm_synth.h"
#include "audio/roland_synth.h"
#include "audio/awe32_synth.h"
#include "audio/gus_synth.h"
#include "audio/sc55_synth.h"
#include "audio/sound_images.h"
#include "audio/soundfont.h"

namespace darker::audio {

struct fm_stream::implementation {
  static constexpr uint64_t pit_frequency{1193180};
  boost::lockfree::spsc_queue<fm_frame, boost::lockfree::capacity<128>> frames;
  fm_frame previous{};
  fm_driver driver;
  fm_synth synth;
  fm_synth right;
  std::unique_ptr<sound_images> music;
  std::unique_ptr<midi_music> sampled_music;
  std::unique_ptr<midi_synth> sampled_synth;
  unsigned int sample_rate;
  midi_sink sampled_sink;
  std::array<std::vector<std::byte>,6> songs;
  std::atomic<int> requested{-1};
  int playing{-1};
  uint64_t phase{0}, period;
  fm_sink sink;

  explicit implementation(unsigned int const sample_rate, fm_backend const backend) : synth{sample_rate,backend}, right{sample_rate,backend}, sample_rate{sample_rate}, sampled_sink{[this](midi_message const message){ sampled_synth->send(message); }}, period{static_cast<uint64_t>(sample_rate) * 23860},
    sink{[this](fm_write const command){ synth.write(command); right.write(command); }} {
    /// The game invokes music every ten 2386-cycle PIT interrupts; driver tempo arithmetic separately uses 5D24
  }

  auto has_music() const->bool {
    return music || sampled_music;
  }

  void effects(fm_frame const &frame) {
    /// Preserve queued effect transitions in producer order, including short retriggers
    for(uint8_t channel{0}; channel < frame.size(); ++channel) {
      auto const &note{frame[channel]};
      if(note.active) {
        auto const program{driver.program(channel,note.patch,note.pitch,note.level,true,
          note.generation != previous[channel].generation)};
        constexpr std::array<uint8_t,9> carriers{0x43,0x44,0x45,0x4b,0x4c,0x4d,0x53,0x54,0x55};
        for(size_t i{0}; i < program.count; ++i) {
          auto left_command{program.writes[i]}, right_command{left_command};
          // The final write sets level; earlier carrier writes may deliberately release the note.
          if(i+1 == program.count && left_command.address == carriers[channel]) {
            if(note.attenuation[0] != 255) left_command.value = note.attenuation[0];
            if(note.attenuation[1] != 255) right_command.value = note.attenuation[1];
          }
          synth.write(left_command);
          right.write(right_command);
        }
      } else if(previous[channel].active) {
        auto const stop{driver.stop(channel)};
        synth.write(stop);
        right.write(stop);
      }
    }
    previous = frame;
  }

  void render(std::span<float> stereo) noexcept {
    /// Sampled music uses its own synthesiser; effects retain synchronised OPL2 paths for independent left/right levels
    if(playing >= 0 && sampled_synth) { sampled_synth->render(stereo); return; }
    std::array<float,1024> right_pcm{};
    while(stereo.size() >= 2) {
      auto const count{std::min(stereo.size() & ~size_t{1},right_pcm.size())};
      synth.render(stereo.first(count));
      right.render(std::span{right_pcm}.first(count));
      for(size_t i{1}; i < count; i += 2) stereo[i] = right_pcm[i];
      stereo = stereo.subspan(count);
    }
  }

  void silence() noexcept {
    /// Relinquish the music channels before reprogramming the same chip for procedural flight effects
    for(uint8_t channel{0}; channel < 9; ++channel) sink(fm_write{static_cast<uint8_t>(0xb0 + channel),0});
    for(uint8_t offset{0}; offset < 22; ++offset) sink(fm_write{static_cast<uint8_t>(0x40 + offset),63});
    sink(fm_write{0xbd,0});
    driver = {};
    previous = {};
  }
};

fm_stream::fm_stream(unsigned int const sample_rate, fm_backend const backend) : state{std::make_unique<implementation>(sample_rate,backend)} {
  /// Construct chip and queue before the device starts consuming audio
}

fm_stream::~fm_stream() = default;

void fm_stream::configure_music(std::span<std::byte const> const driver, std::array<std::vector<std::byte>,6> songs) {
  /// Load and reserve all music storage before starting the audio device
  for(auto const &song : songs) if(song.empty() || song.size() > 65536) throw std::invalid_argument{"Invalid Sound Images music resource size"};
  state->sampled_music.reset();
  state->sampled_synth.reset();
  state->music = std::make_unique<sound_images>(driver);
  state->songs = std::move(songs);
}

void fm_stream::configure_gus_music(std::filesystem::path const &patch_directory, std::array<std::vector<std::byte>,6> songs, unsigned int const ram_kib) {
  for(auto const &song : songs) if(song.empty() || song.size() > 65536) throw std::invalid_argument{"Invalid Gravis music resource size"};
  state->sampled_synth = std::make_unique<gus_synth>(patch_directory, state->sample_rate, ram_kib);
  state->sampled_music = std::make_unique<midi_music>(music_variant::gus);
  state->music.reset();
  state->songs = std::move(songs);
}

void fm_stream::configure_awe32_music(std::filesystem::path const &rom, std::span<std::byte const> const driver, std::array<std::vector<std::byte>,6> songs) {
  for(auto const &song : songs) if(song.empty() || song.size() > 65536) throw std::invalid_argument{"Invalid AWE32 music resource size"};
  state->sampled_synth = std::make_unique<awe32_synth>(rom, driver, state->sample_rate);
  state->sampled_music = std::make_unique<midi_music>(music_variant::awe32);
  state->music.reset();
  state->songs = std::move(songs);
}

void fm_stream::configure_sc55_music(std::filesystem::path const &rom_directory, std::array<std::vector<std::byte>,6> songs) {
  for(auto const &song : songs) if(song.empty() || song.size() > 65536) throw std::invalid_argument{"Invalid SC-55 music resource size"};
  state->sampled_synth = std::make_unique<sc55_synth>(rom_directory, state->sample_rate);
  state->sampled_music = std::make_unique<midi_music>(music_variant::scc1);
  state->music.reset();
  state->songs = std::move(songs);
}

void fm_stream::configure_sampled_music(music_variant const variant, std::filesystem::path const &font, std::array<std::vector<std::byte>,6> songs) {
  /// Load the selected arrangement and SoundFont before starting the device; all synthesis then belongs to its callback
  for(auto const &song : songs) if(song.empty() || song.size() > 65536) throw std::invalid_argument{"Invalid sampled music resource size"};
  state->sampled_synth = std::make_unique<soundfont>(font, state->sample_rate);
  state->sampled_music = std::make_unique<midi_music>(variant);
  state->music.reset();
  state->songs = std::move(songs);
}

void fm_stream::configure_roland_music(std::filesystem::path const &rom_directory, std::span<std::byte const> const driver,
  std::array<std::vector<std::byte>,6> songs, std::filesystem::path const &percussion_font, std::filesystem::path const &percussion_bank, std::filesystem::path const &setup_bank) {
  /// Retain the verified LAPC-I event stream and initialise Munt with the original custom timbres
  for(auto const &song : songs) if(song.empty() || song.size() > 65536) throw std::invalid_argument{"Invalid Roland music resource size"};
  if(!setup_bank.empty() && (!percussion_font.empty() || !percussion_bank.empty()))
    throw std::invalid_argument{"Choose either a full Roland setup bank or a percussion fallback"};
  auto initialisation{setup_bank.empty() ? sysex_messages{} : load_roland_setup(setup_bank)};
  // Reproduce starting the game on a preconfigured device: its own timbres and patches are uploaded last.
  auto const original{lapc_initialisation(driver)};
  initialisation.insert(initialisation.end(), original.begin(), original.end());
  state->sampled_synth = std::make_unique<roland_synth>(rom_directory, state->sample_rate, initialisation, percussion_font, percussion_bank);
  state->sampled_music = std::make_unique<midi_music>(music_variant::lapc1);
  state->music.reset();
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
  /// Render the selected music backend or flight effects; unexpected failures terminate at the noexcept device boundary
  int const requested{state->requested.load(std::memory_order_relaxed)};
  if(requested != state->playing) {
    if(requested < 0) {
      if(state->music) state->music->stop(state->sink);
      if(state->sampled_music) { state->sampled_music->stop(state->sampled_sink); state->sampled_synth->reset(); }
      state->silence();
    }
    state->playing = requested;
    state->phase = 0;
    if(requested >= 0 && state->music) state->music->start(state->songs[static_cast<size_t>(requested)],state->sink);
    if(requested >= 0 && state->sampled_music) {
      state->sampled_synth->reset();
      state->sampled_music->start(state->songs[static_cast<size_t>(requested)], state->sampled_sink);
    }
  }
  fm_frame frame;
  while(state->frames.pop(frame)) if(state->playing < 0 || !state->has_music()) state->effects(frame);
  if(state->playing >= 0 && state->has_music()) {
    size_t cursor{0};
    while(cursor + 1 < stereo.size()) {
      auto const frames{std::min<size_t>((stereo.size() - cursor) / 2,static_cast<size_t>((state->period - state->phase + implementation::pit_frequency - 1) / implementation::pit_frequency))};
      state->render(stereo.subspan(cursor,frames * 2));
      cursor += frames * 2;
      state->phase += frames * implementation::pit_frequency;
      if(state->phase >= state->period) {
        state->phase -= state->period;
        if(state->music) state->music->advance(state->sink);
        else state->sampled_music->advance(state->sampled_sink);
      }
    }
  } else {
    state->render(stereo);
  }
}

} // namespace darker::audio
