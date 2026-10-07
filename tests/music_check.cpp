#include "music_check.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include "audio/fm_stream.h"
#include "audio/sound_images.h"
#include "reference/music_samples.h"

void check_music(darker::resources::archive_set const &archives) {
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
