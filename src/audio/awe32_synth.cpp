#include "audio/awe32_synth.h"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <emu8k.h>
#include <unicorn/unicorn.h>

namespace darker::audio {

struct awe32_synth::implementation {
  EMU8K chip;
  uc_engine *cpu{};
  unsigned int rate{};
  uint64_t phase{};
  std::array<int16_t,2> previous{}, current{};
  bool primed{};

  ~implementation() {
    if(cpu) uc_close(cpu);
  }

  static void check(uc_err result) {
    if(result != UC_ERR_OK) throw std::runtime_error{std::string{"AWE32 driver: "} + uc_strerror(result)};
  }

  void call(uint16_t address, std::span<uint16_t const> arguments = {}) {
    uint16_t stack{0xff00};
    std::array<uint16_t,4> words{0xff00};
    for(size_t i{}; i < arguments.size(); ++i) words[i + 1] = arguments[i];
    check(uc_reg_write(cpu, UC_X86_REG_SP, &stack));
    // x86 arguments are little endian, independently of the host
    std::array<uint8_t,8> bytes{};
    for(size_t i{}; i < words.size(); ++i) {
      bytes[i * 2] = static_cast<uint8_t>(words[i]);
      bytes[i * 2 + 1] = static_cast<uint8_t>(words[i] >> 8);
    }
    check(uc_mem_write(cpu, 0x5ff00, bytes.data(), bytes.size()));
    check(uc_emu_start(cpu, 0x10000 + address, 0x1ff00, 0, 2000000));
    uint16_t ip{};
    check(uc_reg_read(cpu, UC_X86_REG_IP, &ip));
    if(ip != 0xff00) throw std::runtime_error{"AWE32 driver exceeded its instruction budget at " + std::to_string(address)};
  }

  static uint32_t input(uc_engine *, uint32_t port, int size, void *context) {
    auto &self{*static_cast<implementation *>(context)};
    // advance hardware while the native initialiser polls the sample counter
    std::array<int16_t,2> discarded{};
    self.chip.Generate(discarded.data(), 1);
    return size == 1 ? self.chip.Inb(port) : self.chip.Inw(port);
  }

  static void output(uc_engine *, uint32_t port, int size, uint32_t value, void *context) {
    auto &self{*static_cast<implementation *>(context)};
    if(size == 1) self.chip.Outb(port, static_cast<uint8_t>(value));
    else self.chip.Outw(port, static_cast<uint16_t>(value));
  }
};

awe32_synth::awe32_synth(std::filesystem::path const &rom, std::span<std::byte const> const driver, unsigned int const sample_rate)
  : state{std::make_unique<implementation>()} {
  if(sample_rate == 0) throw std::invalid_argument{"AWE32 output sample rate must be positive"};
  if(driver.size() != 35898 || driver[0x1002] != std::byte{0x06} || driver[0x3eea] != std::byte{0x55})
    throw std::invalid_argument{"Unrecognised retail AWE32 driver resource"};
  if(!state->chip.Init(rom.string().c_str(), 512))
    throw std::runtime_error{"AWE32 sample ROM unavailable: " + rom.string() + "; run fetch-assets.sh or supply --awe32-rom"};
  state->chip.ChangeAddr(0x620);
  state->rate = sample_rate;
  implementation::check(uc_open(UC_ARCH_X86, UC_MODE_16, &state->cpu));
  implementation::check(uc_mem_map(state->cpu, 0, 0x100000, UC_PROT_ALL));
  implementation::check(uc_mem_write(state->cpu, 0x10000, driver.data(), driver.size()));
  uint16_t segment{0x1000};
  for(int const reg : {UC_X86_REG_CS, UC_X86_REG_DS, UC_X86_REG_ES})
    implementation::check(uc_reg_write(state->cpu, reg, &segment));
  segment = 0x5000;
  implementation::check(uc_reg_write(state->cpu, UC_X86_REG_SS, &segment));
  uc_hook hook{};
  implementation::check(uc_hook_add(state->cpu, &hook, UC_HOOK_INSN, reinterpret_cast<void *>(implementation::input), state.get(), 1, 0, UC_X86_INS_IN));
  implementation::check(uc_hook_add(state->cpu, &hook, UC_HOOK_INSN, reinterpret_cast<void *>(implementation::output), state.get(), 1, 0, UC_X86_INS_OUT));
  // execute only the original embedded synthesis library; sequencing remains C++
  state->call(0x1002);
}

awe32_synth::~awe32_synth() = default;

void awe32_synth::send(midi_message const message) noexcept {
  uint16_t const channel{static_cast<uint16_t>(message.status & 15)};
  std::array<uint16_t,3> const arguments{message.second, message.first, channel};
  switch(message.status & 0xf0) {
  case 0x80: state->call(0x3e0a, arguments); break;
  case 0x90: state->call(0x3eea, arguments); break;
  case 0xb0: state->call(0x4314, arguments); break;
  case 0xc0: state->call(0x4454, std::array<uint16_t,2>{message.first, channel}); break;
  case 0xe0: state->call(0x45fe, arguments); break;
  default: break;
  }
}

void awe32_synth::reset() noexcept {
  // the original sequencer stops voices without reinitialising channel controllers
  for(uint8_t channel{}; channel < 16; ++channel) send({static_cast<uint8_t>(0xb0 | channel), 123, 0});
}

void awe32_synth::render(std::span<float> const stereo) noexcept {
  if(!state->primed) {
    state->chip.Generate(state->current.data(), 1);
    state->previous = state->current;
    state->primed = true;
  }
  for(size_t i{}; i + 1 < stereo.size(); i += 2) {
    while(state->phase >= state->rate) {
      state->phase -= state->rate;
      state->previous = state->current;
      state->chip.Generate(state->current.data(), 1);
    }
    float const fraction{static_cast<float>(state->phase) / static_cast<float>(state->rate)};
    for(size_t channel{}; channel < 2; ++channel)
      stereo[i + channel] = (state->previous[channel] + fraction * (state->current[channel] - state->previous[channel])) / 32768.0f;
    state->phase += 44100;
  }
}

} // namespace darker::audio
