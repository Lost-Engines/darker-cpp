#include "audio/sound_images.h"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace darker::audio {

sound_images::sound_images(std::span<std::byte const> const bytes) : driver{bytes.begin(),bytes.end()} {
  /// Retain the original SB driver tables; no General MIDI instrument substitution is involved
  sequence.reserve(65536);
  for(uint8_t i{0}; i < tracks.size(); ++i) tracks[i].channel = i;
  if(driver.size() != 0x2c40) throw std::invalid_argument{"Unexpected Sound Images SB driver size"};
}

uint8_t sound_images::read(size_t const offset) const {
  /// Instrument, pitch and attenuation lookups are bounded by the original driver resource
  return std::to_integer<uint8_t>(driver.at(offset));
}

uint16_t sound_images::word(size_t const offset) const {
  /// Tables retain the original little-endian words
  return static_cast<uint16_t>(read(offset) | (read(offset + 1) << 8));
}

uint8_t sound_images::byte(track &part) const {
  /// Reject a sequence cursor outside its owning resource
  return std::to_integer<uint8_t>(sequence.at(part.cursor++));
}

uint32_t sound_images::delta(track &part) const {
  /// 05F0 decodes at most five seven-bit groups, with a continued fifth byte parking the track
  uint32_t result{0};
  for(unsigned int i{0}; i < 5; ++i) {
    auto const value{byte(part)};
    result = (result << 7) | (value & 127);
    if(!(value & 128)) return result;
  }
  return 0xffffffff;
}

void sound_images::timing() {
  /// 065A divides the PIT frequency times 60 by Darker's supplied 5D24 timer period before forming Q16 increments
  constexpr uint32_t ticks_per_minute{0x04446390 / 0x5d24};
  increment = static_cast<uint32_t>((static_cast<uint64_t>(division) * tempo << 16) / ticks_per_minute);
}

void sound_images::write(uint8_t const address, uint8_t const value, fm_sink const &sink) {
  /// 0856 suppresses unchanged OPL registers
  if(registers[address] == value) return;
  registers[address] = value;
  sink({address,value});
}

void sound_images::start(std::span<std::byte const> const song, fm_sink const &sink) {
  /// Recreate the original channel setup and load the single song descriptor through 03B2/0431
  if(song.size() > 65536) throw std::invalid_argument{"Sound Images song exceeds its DOS segment"};
  sequence.assign(song.begin(),song.end());
  for(size_t i{0}; i < registers.size(); ++i) registers[i] = read(0x138e + i);
  for(size_t p{0x1af2}; word(p); p += 2) {
    auto const address{read(p)}, value{read(p + 1)};
    registers[address] = value;
    sink({address,value});
  }
  for(auto &part : tracks) { release(part,sink); part.active = false; }
  auto const song_word{[&](size_t const p){ return std::to_integer<uint8_t>(sequence.at(p)) | (std::to_integer<uint8_t>(sequence.at(p+1)) << 8); }};
  if(sequence.empty() || sequence.front() != std::byte{1}) throw std::invalid_argument{"Sound Images resource must contain one song"};
  auto const descriptor{song_word(static_cast<size_t>(song_word(1)))};
  division = std::to_integer<uint8_t>(sequence.at(descriptor));
  tempo = std::to_integer<uint8_t>(sequence.at(descriptor + 1));
  auto const count{std::to_integer<uint8_t>(sequence.at(descriptor + 2))};
  if(count > 9) throw std::invalid_argument{"Sound Images FM music exceeds nine channels"};
  for(uint8_t i{0}; i < count; ++i) {
    auto &part{tracks[i]};
    part.bend_range = 2;
    part.bend = 64;
    part.percussion = 0;
    part.program = 255;
    part.volume = 127;
    part.cursor = part.loop = static_cast<size_t>(song_word(descriptor + 3 + i * 2));
    part.delay = delta(part);
    part.active = true;
    level(part,sink);
  }
  timing();
}

void sound_images::instrument(track &part, fm_sink const &sink) {
  /// C093 writes the two-operator instrument, muting both operators before replacing their envelopes
  auto const low{static_cast<uint8_t>(part.operators)}, high{static_cast<uint8_t>(part.operators >> 8)};
  write(high | 0x40,63,sink);
  write(low | 0x40,63,sink);
  for(auto const &[offset,base] : {std::pair{0,0x60}, {2,0x80}, {6,0xe0}}) {
    write(static_cast<uint8_t>(low + base),read(part.instrument + offset),sink);
    write(static_cast<uint8_t>(high + base),read(part.instrument + offset + 1),sink);
  }
  write(static_cast<uint8_t>(part.channel + 0xc0),read(part.instrument + 9),sink);
  write(low | 0x20,read(part.instrument + 4),sink);
  write(high | 0x20,read(part.instrument + 5),sink);
}

void sound_images::pitch(track &part, uint8_t const note, fm_sink const &sink) {
  /// 0D71 interpolates pitch through the driver's 768-entry frequency table and original bend range
  int bend{static_cast<int>(part.bend) - 64};
  if(bend >= 63) ++bend;
  auto const units{static_cast<uint16_t>(note * 64 + bend * part.bend_range)};
  int block{units / 768 - 1};
  auto const remainder{units % 768};
  auto frequency{word(0x14ba + remainder * 2)};
  if(block < 0 && remainder < 448) {
    ++block;
    frequency = static_cast<uint16_t>(std::bit_cast<int16_t>(frequency) >> 1);
  } else frequency = static_cast<uint16_t>((frequency & 0x7ff) + (block * 1024));
  write(part.channel | 0xa0,static_cast<uint8_t>(frequency),sink);
  part.key = static_cast<uint8_t>((frequency >> 8) | 0x20);
  write(part.channel | 0xb0,part.key,sink);
}

void sound_images::level(track const &part, fm_sink const &sink) {
  /// 0E67 combines instrument level, note velocity, channel volume and unity master gain before logarithmic attenuation
  auto const scaled{[&](uint8_t const value){
    uint32_t result{static_cast<uint8_t>(value + 1) * static_cast<uint32_t>(static_cast<uint8_t>(part.velocity + 1))};
    result = (result >> 1) * 8;
    result >>= 1;
    result = (result * (static_cast<uint8_t>(part.volume + 1) * 2u)) >> 8;
    return static_cast<uint8_t>((result * 257) >> 16);
  }};
  auto const carrier{scaled(read(part.instrument + 11))};
  auto const scaling{read(part.instrument + 12)};
  write(static_cast<uint8_t>((part.operators >> 8) | 0x40),static_cast<uint8_t>(((scaling & 3) << 6) | read(0x1b7a + carrier)),sink);
  auto const modulator{read(part.instrument + 10)};
  auto const volume{(read(part.instrument + 9) & 1) ? scaled(modulator) : modulator};
  write(static_cast<uint8_t>(part.operators | 0x40),static_cast<uint8_t>(((scaling << 2) & 0xc0) | read(0x1b7a + volume)),sink);
}

void sound_images::release(track &part, fm_sink const &sink) {
  /// C029 removes the key-on bit without changing pitch or envelopes
  if(!(part.key & 0x20)) return;
  part.key &= 0xdf;
  write(part.channel | 0xb0,part.key,sink);
}

void sound_images::event(track &part, fm_sink const &sink) {
  /// 030C dispatches Sound Images events, including the SB-specific percussion map
  auto const op{byte(part)};
  if(op < 128) {
    part.note = op;
    release(part,sink);
    uint8_t note;
    if(part.percussion) {
      auto const patch{read(0x23c0 + op)};
      if(patch != part.program) {
        part.program = patch;
        part.instrument = static_cast<uint16_t>(0x2440 + patch * 16);
        instrument(part,sink);
      }
      note = static_cast<uint8_t>(read(part.instrument + 8) + 24);
      part.note = note;
    } else note = static_cast<uint8_t>(op - 24 + read(part.instrument + 8));
    pitch(part,note,sink);
    part.velocity = byte(part);
    level(part,sink);
    return;
  }
  if(op < 0x90) {
    part.channel = op & 15;
    if(part.channel >= 9) throw std::invalid_argument{"Unsupported digital music channel"};
    part.operators = word(0x1ad2 + part.channel * 2);
    return;
  }
  switch(op) {
  case 0x90: if(byte(part) == part.note) release(part,sink); break;
  case 0x91: release(part,sink); part.active = false; break;
  case 0x92:
    part.bend = 64;
    part.program = byte(part);
    part.instrument = static_cast<uint16_t>(0x2440 + part.program * 16);
    part.bend_range = read(part.instrument + 15);
    instrument(part,sink);
    break;
  case 0x93: tempo = byte(part); timing(); break;
  case 0x94: part.percussion = byte(part); break;
  case 0x95:
    part.bend = byte(part);
    if(part.key & 0x20) pitch(part,static_cast<uint8_t>(part.note + read(part.instrument + 8) - 24),sink);
    break;
  case 0x96: part.volume = byte(part); break;
  case 0x97: part.percussion = byte(part); break;
  case 0x98: part.cursor = part.loop; break;
  case 0x99: release(part,sink); break;
  case 0x9a: part.active = false; break;
  case 0x9b: byte(part); break;
  case 0x9c: part.loop = part.cursor; break;
  case 0x9d: byte(part); byte(part); break;
  case 0xff: break;
  default: throw std::invalid_argument{"Unsupported Sound Images event"};
  }
}

void sound_images::stop(fm_sink const &sink) {
  /// 0A7E requests a stop then invokes the driver once, retaining its fractional clock for the next song
  for(auto &part : tracks) { release(part,sink); part.active = false; }
  clock_fraction = (clock_fraction + increment) & 65535;
}

void sound_images::advance(fm_sink const &sink) {
  /// 0295 advances tracks in source order using one shared fixed-point tempo accumulator
  auto const total{clock_fraction + increment};
  clock_fraction = total & 65535;
  auto const elapsed{total >> 16};
  for(auto &part : tracks) {
    if(!part.active) continue;
    part.delay -= elapsed;
    for(unsigned int count{0}; part.active && part.delay < 0; ++count) {
      if(count >= 65536) throw std::invalid_argument{"Sound Images track failed to yield"};
      event(part,sink);
      if(part.active) part.delay += delta(part);
    }
  }
}

} // namespace darker::audio
