#include "music_check.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include "audio/fm_stream.h"
#include "audio/roland_patches.h"
#include "audio/roland_synth.h"
#include "reference/roland_initialisation.h"
#include "audio/sound_images.h"
#include "reference/midi_music_samples.h"
#include "reference/midi_transition_samples.h"
#include "reference/music_samples.h"

void check_music(darker::resources::archive_set const &archives) {
  auto const upload{darker::audio::lapc_initialisation(archives.load({0,35}))};
  std::vector<uint8_t> bytes;
  for(auto const &message : upload) bytes.insert(bytes.end(), message.begin(), message.end());
  if(!std::ranges::equal(bytes, darker::test_reference::roland_initialisation))
    throw std::runtime_error{"Roland instrument upload differs from native driver"};
  if(!std::string_view{DARKER_TEST_MT32_ROM_DIR}.empty() && !std::string_view{DARKER_TEST_SOUNDFONT}.empty()) {
    for(uint8_t const key : {uint8_t{42}, uint8_t{52}, uint8_t{55}, uint8_t{57}, uint8_t{58}}) {
      darker::audio::roland_synth original{DARKER_TEST_MT32_ROM_DIR, 48000, upload};
      darker::audio::roland_synth supplemented{DARKER_TEST_MT32_ROM_DIR, 48000, upload, DARKER_TEST_SOUNDFONT};
      original.send({0x99, key, 110});
      supplemented.send({0x99, key, 110});
      std::array<float,512> original_pcm{}, supplemented_pcm{};
      double difference{0};
      for(int block{0}; block < 100; ++block) {
        original.render(original_pcm);
        supplemented.render(supplemented_pcm);
        for(size_t i{0}; i < original_pcm.size(); ++i) {
          if(!std::isfinite(supplemented_pcm[i])) throw std::runtime_error{"Roland percussion fallback produced non-finite PCM"};
          difference += std::abs(supplemented_pcm[i] - original_pcm[i]);
        }
      }
      if(key == 42 && difference != 0) throw std::runtime_error{"Roland percussion fallback alters mapped notes"};
      if(key != 42 && difference < 1) throw std::runtime_error{"Roland percussion fallback did not sound an unmapped note"};
      original.reset();
      supplemented.reset();
      // Allow the SoundFont quick-release envelope to finish after all-sounds-off.
      for(int block{0}; block < 20; ++block) {
        original.render(original_pcm);
        supplemented.render(supplemented_pcm);
      }
      if(original_pcm != supplemented_pcm) throw std::runtime_error{"Roland percussion fallback leaves voices active after reset"};
    }
    std::cout << "Roland fallback sounds all four missing percussion keys, preserves mapped notes and stops on reset." << std::endl;
  }
  if(!std::string_view{DARKER_TEST_MT32_ROM_DIR}.empty()) {
    std::array<std::vector<std::byte>,6> roland_songs;
    for(unsigned int group{0}; group < 6; ++group) roland_songs[group] = archives.load({0,40 + group * 5});
    darker::audio::fm_stream stream{48000};
    stream.configure_roland_music(DARKER_TEST_MT32_ROM_DIR, archives.load({0,35}), std::move(roland_songs));
    std::array<float,512> pcm{};
    for(int group{0}; group < 6; ++group) {
      stream.select_music(group);
      double energy{0};
      for(int block{0}; block < 1000; ++block) {
        stream.render(pcm);
        for(float const value : pcm) {
          if(!std::isfinite(value)) throw std::runtime_error{"Roland music produced non-finite PCM"};
          energy += std::abs(value);
        }
      }
      if(energy < 1) throw std::runtime_error{"Roland arrangement is silent"};
    }
  } else std::cout << "Roland PCM check omitted: configure DARKER_TEST_MT32_ROM_DIR to enable it." << std::endl;
  /// Compare complete timed OPL streams with native 0295 execution across repeated songs
  auto const driver{archives.load({0,33})};
  std::array<std::vector<std::byte>,6> songs;
  for(size_t group{0}; group < songs.size(); ++group) {
    auto const &sample{darker::test_reference::music_samples[group]};
    songs[group] = archives.load({0,sample.resource});
    darker::audio::sound_images music{driver};
    uint64_t hash{0xcbf29ce484222325};
    unsigned int tick{0}, writes{0};
    darker::audio::fm_sink const sink{[&](auto const command){
      for(unsigned int const byte : {tick & 255,tick >> 8,static_cast<unsigned int>(command.address),static_cast<unsigned int>(command.value)}) hash = (hash ^ byte) * 0x100000001b3;
      ++writes;
    }};
    music.start(songs[group],sink);
    for(tick = 0; tick < sample.ticks; ++tick) music.advance(sink);
    if(hash != sample.fingerprint || writes != sample.writes) throw std::runtime_error{"Sound Images OPL trace differs from native driver for group " + std::to_string(group)};
  }
  for(auto const &sample : darker::test_reference::music_transition_samples) {
    darker::audio::sound_images music{driver};
    uint64_t hash{0xcbf29ce484222325};
    unsigned int tick{0}, writes{0};
    darker::audio::fm_sink const sink{[&](auto const command){
      for(unsigned int const byte : {tick & 255,tick >> 8,static_cast<unsigned int>(command.address),static_cast<unsigned int>(command.value)}) hash = (hash ^ byte) * 0x100000001b3;
      ++writes;
    }};
    music.start(songs[sample.source],sink);
    for(tick = 0; tick < 1280; ++tick) {
      if(tick == 512 && sample.pause) music.stop(sink);
      if(tick == (sample.pause ? 768u : 512u)) music.start(songs[sample.target],sink);
      if(!sample.pause || tick < 512 || tick >= 768) music.advance(sink);
    }
    if(hash != sample.fingerprint || writes != sample.writes) throw std::runtime_error{"Music selection or stop/resume differs from the native driver"};
  }
  for(auto const &sample : darker::test_reference::midi_music_samples) {
    darker::audio::midi_music music{static_cast<darker::audio::music_variant>(sample.variant)};
    uint64_t hash{0xcbf29ce484222325};
    unsigned int tick{0}, events{0};
    darker::audio::midi_sink const sink{[&](darker::audio::midi_message const message){
      for(unsigned int const byte : {tick & 255, tick >> 8, static_cast<unsigned int>(message.status),
        static_cast<unsigned int>(message.first), static_cast<unsigned int>(message.second)}) hash = (hash ^ byte) * 0x100000001b3;
      ++events;
    }};
    music.start(archives.load({0, 38 + sample.variant + sample.group * 5}), sink);
    for(tick = 0; tick < sample.ticks; ++tick) music.advance(sink);
    if(events != sample.events || hash != sample.fingerprint) throw std::runtime_error{"Sampled music differs from native driver: variant "
      + std::to_string(sample.variant) + " group " + std::to_string(sample.group) + " events " + std::to_string(events) + "/" + std::to_string(sample.events)};
  }
  for(auto const &sample : darker::test_reference::midi_transition_samples) {
    darker::audio::midi_music music{static_cast<darker::audio::music_variant>(sample.variant)};
    uint64_t hash{0xcbf29ce484222325};
    unsigned int tick{0}, events{0};
    darker::audio::midi_sink const sink{[&](darker::audio::midi_message const message){
      for(unsigned int const byte : {tick & 255, tick >> 8, static_cast<unsigned int>(message.status),
        static_cast<unsigned int>(message.first), static_cast<unsigned int>(message.second)}) hash = (hash ^ byte) * 0x100000001b3;
      ++events;
    }};
    music.start(archives.load({0, 38 + sample.variant}), sink);
    for(tick = 0; tick < 1280; ++tick) {
      if(tick == 512 && sample.pause) music.stop(sink);
      if(tick == (sample.pause ? 768u : 512u)) music.start(archives.load({0, 43 + sample.variant}), sink);
      if(!sample.pause || tick < 512 || tick >= 768) music.advance(sink);
    }
    if(events != sample.events || hash != sample.fingerprint) throw std::runtime_error{"Sampled music transition differs from native driver: variant " + std::to_string(sample.variant)};
  }
  if(std::string_view{DARKER_TEST_SOUNDFONT}.empty()) std::cout << "Sampled PCM check omitted: configure DARKER_TEST_SOUNDFONT to enable it." << std::endl;
  else for(unsigned int variant{1}; variant < 5; ++variant) {
    std::array<std::vector<std::byte>,6> sampled_songs;
    for(unsigned int group{0}; group < 6; ++group) sampled_songs[group] = archives.load({0, 38 + variant + group * 5});
    darker::audio::fm_stream stream{48000};
    stream.configure_sampled_music(static_cast<darker::audio::music_variant>(variant), DARKER_TEST_SOUNDFONT, std::move(sampled_songs));
    std::array<float,512> pcm{};
    for(int group{0}; group < 6; ++group) {
      stream.select_music(group);
      double energy{0};
      for(int block{0}; block < 1000; ++block) {
        stream.render(pcm);
        for(float const value : pcm) {
          if(!std::isfinite(value)) throw std::runtime_error{"Sampled music produced non-finite PCM"};
          energy += std::abs(value);
        }
      }
      if(energy < 1) throw std::runtime_error{"Sampled music arrangement is silent"};
    }
    stream.select_music(-1);
    darker::audio::fm_frame effect{};
    effect[0] = {.pitch{1200}, .level{8192}, .generation{1}, .patch{1}, .active{true}};
    if(!stream.publish(effect)) throw std::runtime_error{"Failed to enqueue effects after sampled music"};
    double energy{0};
    for(int block{0}; block < 100; ++block) {
      stream.render(pcm);
      for(float const value : pcm) energy += std::abs(value);
    }
    if(energy < 1) throw std::runtime_error{"OPL effects failed to resume after sampled music"};
  }
  std::cout << "All 24 sampled arrangements match native timed events across repeated loops." << std::endl;
  if(!std::string_view{DARKER_TEST_SOUNDFONT}.empty()) std::cout << "All 24 arrangements render finite, audible SoundFont PCM and return to OPL effects." << std::endl;
  // Changing device block sizes must not alter music timing or sample output.
  auto const render{[&](size_t const block){
    darker::audio::fm_stream stream{48000};
    stream.configure_music(driver,songs);
    stream.select_music(0);
    std::array<float,2048> pcm{};
    uint64_t hash{0xcbf29ce484222325};
    double energy{0};
    for(size_t offset{0}; offset < 48000 * 20;) {
      auto const frames{std::min(block,48000 * 20 - offset)};
      auto output{std::span{pcm}.first(frames * 2)};
      stream.render(output);
      for(float const value : output) {
        if(!std::isfinite(value)) throw std::runtime_error{"Music produced non-finite PCM"};
        hash = (hash ^ std::bit_cast<uint32_t>(value)) * 0x100000001b3;
        energy += std::abs(value);
      }
      offset += frames;
    }
    if(energy < 1) throw std::runtime_error{"Music stream is silent"};
    stream.select_music(-1);
    stream.render(pcm);
    return hash;
  }};
  if(render(512) != render(257)) throw std::runtime_error{"Music timing depends on the audio device block size"};
  std::cout << "Six Sound Images groups match native timed OPL writes for 16,384 ticks each; twelve selection/stop/resume traces match; PCM is independent of callback block size." << std::endl;
}
