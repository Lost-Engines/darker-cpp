#include "audio/midi_music.h"
#include <stdexcept>

namespace darker::audio {

midi_music::midi_music(music_variant const selected) : variant{selected} {
  /// Sampled drivers share the sequence grammar, but SCC-1 restricts last-note release to channels 0–11
  if(selected == music_variant::soundblaster) throw std::invalid_argument{"Sound Blaster music requires the OPL sequencer"};
  sequence.reserve(65536);
  for(uint8_t i{0}; i < tracks.size(); ++i) tracks[i].channel = i;
}

auto midi_music::byte(track &part)->uint8_t {
  return std::to_integer<uint8_t>(sequence.at(part.cursor++));
}

auto midi_music::delta(track &part)->uint32_t {
  uint32_t value{0};
  for(unsigned int i{0}; i < 5; ++i) {
    auto const next{byte(part)};
    value = (value << 7) | (next & 127);
    if(!(next & 128)) return value;
  }
  return 0xffffffff;
}

void midi_music::timing() {
  /// SCC-1 0668, LAPC-I 0669, GUS 0658 and AWE32 0FA0 use the same Q16 tempo arithmetic
  constexpr uint32_t ticks_per_minute{0x04446390 / 0x5d24};
  increment = static_cast<uint32_t>((static_cast<uint64_t>(division) * tempo << 16) / ticks_per_minute);
}

void midi_music::start(std::span<std::byte const> const song, midi_sink const &sink) {
  if(song.empty() || song.size() > 65536 || song.front() != std::byte{1}) throw std::invalid_argument{"Expected one Sound Images MIDI song"};
  sequence.assign(song.begin(), song.end());
  auto const word{[&](size_t const p){ return std::to_integer<uint8_t>(sequence.at(p)) | (std::to_integer<uint8_t>(sequence.at(p + 1)) << 8); }};
  auto const descriptor{static_cast<size_t>(word(word(1)))};
  division = std::to_integer<uint8_t>(sequence.at(descriptor));
  tempo = std::to_integer<uint8_t>(sequence.at(descriptor + 1));
  auto const count{std::to_integer<uint8_t>(sequence.at(descriptor + 2))};
  if(!division || !tempo || count > tracks.size()) throw std::invalid_argument{"Invalid Sound Images MIDI descriptor"};
  track_count = count;
  for(auto &part : tracks) {
    if(part.active) sink({static_cast<uint8_t>(0xb0 | part.channel), 123, 0});
    part.active = false;
  }
  for(uint8_t i{0}; i < count; ++i) {
    auto &part{tracks[i]};
    part.cursor = part.loop = static_cast<size_t>(word(descriptor + 3 + i * 2));
    part.delay = delta(part);
    part.active = true;
  }
  if(variant != music_variant::gus) for(uint8_t i{0}; i < count; ++i) sink({static_cast<uint8_t>(0xb0 | tracks[i].channel), 7, 127});
  timing();
}

void midi_music::event(track &part, midi_sink const &sink) {
  auto const op{byte(part)};
  auto const send{[&](uint8_t const status, uint8_t const first, uint8_t const second = 0) {
    sink({static_cast<uint8_t>(status | part.channel), first, second});
  }};
  if(op < 128) {
    part.note = op;
    send(0x90, op, byte(part));
    return;
  }
  if(op < 0x90) { part.channel = op & 15; return; }
  switch(op) {
  case 0x90: send(0x90, byte(part), 0); break;
  case 0x91: send(0xb0, 123, 0); part.active = false; break;
  case 0x92: send(0xc0, byte(part)); break;
  case 0x93: tempo = byte(part); timing(); break;
  case 0x94: break;
  case 0x95: send(0xe0, 0, byte(part)); break;
  case 0x96: send(0xb0, 7, byte(part)); break;
  case 0x97: byte(part); break;
  case 0x98: part.cursor = part.loop; break;
  case 0x99:
    if(variant != music_variant::scc1 || part.channel <= 11) send(0x90, part.note, 0);
    break;
  case 0x9a: part.active = false; break;
  case 0x9b: send(0xb0, 10, byte(part)); break;
  case 0x9c: part.loop = part.cursor; break;
  case 0x9d: {
    auto const controller{byte(part)};
    send(0xb0, controller, byte(part));
    break;
  }
  case 0xff: break;
  default: throw std::invalid_argument{"Unsupported Sound Images MIDI command"};
  }
}

void midi_music::advance(midi_sink const &sink) {
  auto const total{clock_fraction + increment};
  clock_fraction = total & 65535;
  auto const elapsed{total >> 16};
  for(auto &part : tracks) {
    if(!part.active) continue;
    part.delay -= elapsed;
    for(unsigned int count{0}; part.active && part.delay < 0; ++count) {
      if(count >= 65536) throw std::invalid_argument{"Sound Images MIDI track failed to yield"};
      event(part, sink);
      if(part.active) part.delay += delta(part);
    }
  }
}

void midi_music::stop(midi_sink const &sink) {
  for(auto &part : std::span{tracks}.first(track_count)) {
    sink({static_cast<uint8_t>(0xb0 | part.channel), 123, 0});
    part.active = false;
  }
  clock_fraction = (clock_fraction + increment) & 65535;
}

} // namespace darker::audio
