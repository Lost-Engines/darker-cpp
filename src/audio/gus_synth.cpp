#include "audio/gus_synth.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <unicorn/unicorn.h>
#include "audio/gf1/device.h"

namespace darker::audio {
namespace {
auto read_file(std::filesystem::path const &path)->std::vector<uint8_t> {
  std::ifstream file{path, std::ios::binary | std::ios::ate};
  auto const size{file.tellg()};
  if(!file || size < 0 || size > 1048576) throw std::runtime_error{"Missing or invalid Gravis file: " + path.string()};
  std::vector<uint8_t> bytes(static_cast<size_t>(size));
  file.seekg(0);
  if(!file.read(reinterpret_cast<char *>(bytes.data()), size)) throw std::runtime_error{"Cannot read Gravis file: " + path.string()};
  return bytes;
}
auto word(std::span<uint8_t const> bytes, size_t offset)->uint16_t {
  if(offset + 2 > bytes.size()) throw std::runtime_error{"Truncated UltraMID executable"};
  return static_cast<uint16_t>(bytes[offset] | bytes[offset + 1] << 8);
}
} // anonymous namespace

struct gus_synth::implementation {
  uc_engine *cpu{};
  gf1_device chip;
  std::filesystem::path directory;
  struct opened_file { std::vector<uint8_t> bytes; size_t offset{}; };
  std::map<uint16_t,opened_file> files;
  std::array<uint8_t,65536> ports{};
  uint16_t next_handle{5}, allocation{0x5000}, entry_segment{}, entry_offset{};
  uint16_t dma_address{}, dma_length{};
  uint8_t dma_page{}, dma_flip{};
  uint32_t reads{};
  unsigned int rate{}, pending_irq{};
  uint64_t phase{}, bios_phase{};
  std::array<int16_t,2> previous{}, current{};
  bool boot{true}, resident{}, primed{};
  std::string failure;

  explicit implementation(std::filesystem::path const &path, unsigned int sample_rate)
    : chip{[this](std::span<std::byte> destination) {
        check(uc_mem_read(cpu, static_cast<uint32_t>(dma_page) * 65536 + dma_address, destination.data(), destination.size()));
      }}, directory{path}, rate{sample_rate} {}

  ~implementation() {
    if(cpu) uc_close(cpu);
  }

  static void check(uc_err result) {
    if(result != UC_ERR_OK) throw std::runtime_error{std::string{"UltraMID: "} + uc_strerror(result)};
  }
  auto reg(int name)->uint16_t {
    uint16_t value{};
    check(uc_reg_read(cpu, name, &value));
    return value;
  }
  void reg(int name, uint16_t value) {
    check(uc_reg_write(cpu, name, &value));
  }
  auto memory_word(uint32_t address)->uint16_t {
    std::array<uint8_t,2> bytes{};
    check(uc_mem_read(cpu, address, bytes.data(), bytes.size()));
    return word(bytes, 0);
  }
  void memory_word(uint32_t address, uint16_t value) {
    std::array<uint8_t,2> const bytes{static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8)};
    check(uc_mem_write(cpu, address, bytes.data(), bytes.size()));
  }
  auto string(uint32_t address)->std::string {
    std::array<char,512> bytes{};
    check(uc_mem_read(cpu, address, bytes.data(), bytes.size()));
    auto const end{std::find(bytes.begin(), bytes.end(), '\0')};
    if(end == bytes.end()) throw std::runtime_error{"Unterminated UltraMID filename"};
    return {bytes.begin(), end};
  }
  void carry(bool set) {
    reg(UC_X86_REG_FLAGS, static_cast<uint16_t>((reg(UC_X86_REG_FLAGS) & ~1) | set));
  }
  void interrupt(unsigned int number) {
    auto const ax{reg(UC_X86_REG_AX)}, bx{reg(UC_X86_REG_BX)}, cx{reg(UC_X86_REG_CX)}, dx{reg(UC_X86_REG_DX)}, ds{reg(UC_X86_REG_DS)};
    auto const ah{ax >> 8}, al{ax & 255};
    carry(false);
    if(number == 0x10) return; // discard the TSR's BIOS console banner
    if(number == 0x2f) { reg(UC_X86_REG_AX, 0); return; }
    if(number == 0x1a) { reg(UC_X86_REG_CX, 0); reg(UC_X86_REG_DX, static_cast<uint16_t>(reads / 100)); reg(UC_X86_REG_AX, 0); return; }
    if(number != 0x21) throw std::runtime_error{"Unexpected UltraMID interrupt " + std::to_string(number)};
    switch(ah) {
    case 0x09: break;
    case 0x30: reg(UC_X86_REG_AX, 5); reg(UC_X86_REG_BX, 0); reg(UC_X86_REG_CX, 0); break;
    case 0x35: reg(UC_X86_REG_BX, memory_word(al * 4)); reg(UC_X86_REG_ES, memory_word(al * 4 + 2)); break;
    case 0x25: memory_word(al * 4, dx); memory_word(al * 4 + 2, ds); break;
    case 0x4a: case 0x49: break; // private conventional-memory arena; reclaimed with this instance
    case 0x48:
      if(static_cast<unsigned int>(allocation) + bx >= 0x9000) throw std::runtime_error{"UltraMID exhausted conventional memory"};
      reg(UC_X86_REG_AX, allocation);
      allocation = static_cast<uint16_t>(allocation + bx + 1);
      break;
    case 0x51: case 0x62: reg(UC_X86_REG_BX, 0x1000); break;
    case 0x3d:
      {
        auto name{string(static_cast<uint32_t>(ds) * 16 + dx)};
        std::replace(name.begin(), name.end(), '\\', '/');
        name = name.substr(name.find_last_of('/') + 1);
        for(auto &letter : name) letter = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
        if(name != "ULTRAMID.INI" && !name.ends_with(".PAT")) throw std::runtime_error{"Unexpected UltraMID file request: " + name};
        files.emplace(next_handle, opened_file{read_file(directory / name)});
        reg(UC_X86_REG_AX, next_handle++);
      }
      break;
    case 0x3f:
      {
        auto &file{files.at(bx)};
        auto const count{std::min<size_t>(cx, file.bytes.size() - std::min(file.offset, file.bytes.size()))};
        if(count) check(uc_mem_write(cpu, static_cast<uint32_t>(ds) * 16 + dx, file.bytes.data() + file.offset, count));
        file.offset += count;
        reg(UC_X86_REG_AX, static_cast<uint16_t>(count));
      }
      break;
    case 0x3e: files.erase(bx); break;
    case 0x42:
      {
        auto &file{files.at(bx)};
        int64_t const offset{static_cast<int32_t>(static_cast<uint32_t>(cx) << 16 | dx)};
        int64_t const target{offset + static_cast<int64_t>(al == 1 ? file.offset : al == 2 ? file.bytes.size() : 0)};
        if(target < 0 || target > 1048576) throw std::runtime_error{"Invalid UltraMID file seek"};
        file.offset = static_cast<size_t>(target);
        reg(UC_X86_REG_AX, static_cast<uint16_t>(target));
        reg(UC_X86_REG_DX, static_cast<uint16_t>(target >> 16));
      }
      break;
    case 0x40: reg(UC_X86_REG_AX, cx); break;
    case 0x31: resident = true; check(uc_emu_stop(cpu)); break;
    case 0x4c: throw std::runtime_error{"UltraMID initialisation failed, exit " + std::to_string(al)};
    case 0x2c: reg(UC_X86_REG_CX, 0); reg(UC_X86_REG_DX, 0); break;
    case 0x2a: reg(UC_X86_REG_CX, 1995); reg(UC_X86_REG_DX, 0x0101); break;
    case 0x44: reg(UC_X86_REG_DX, bx < 5 ? 0x80 : 0); break;
    case 0x34: reg(UC_X86_REG_ES, 0x9000); reg(UC_X86_REG_BX, 0); break;
    case 0x19: reg(UC_X86_REG_AX, 2); break;
    case 0x47:
      {
        constexpr char path[]{"ULTRASND"};
        check(uc_mem_write(cpu, static_cast<uint32_t>(ds) * 16 + reg(UC_X86_REG_SI), path, sizeof(path)));
      }
      break;
    case 0x58: reg(UC_X86_REG_AX, 0); break;
    default: throw std::runtime_error{"Unsupported UltraMID DOS service " + std::to_string(ah)};
    }
  }
  void run(uint32_t start, uint32_t end, size_t budget) {
    check(uc_emu_start(cpu, start, end, 0, budget));
    if(!failure.empty()) throw std::runtime_error{failure};
  }
  void call(uint16_t segment, uint16_t offset, uint16_t ax = 0, uint16_t cx = 0) {
    reg(UC_X86_REG_SS, 0x9000);
    reg(UC_X86_REG_SP, 0xff00);
    reg(UC_X86_REG_AX, ax);
    reg(UC_X86_REG_CX, cx);
    reg(UC_X86_REG_CS, segment);
    memory_word(0x9ff00, 0);
    memory_word(0x9ff02, 0xf000);
    memory_word(0x9ff04, 0x202); // also supports IRET from the resident interrupt handlers
    run(static_cast<uint32_t>(segment) * 16 + offset, 0xf0000, 100000);
    if(reg(UC_X86_REG_CS) != 0xf000) throw std::runtime_error{"UltraMID callback " + std::to_string(offset) + " exceeded its instruction budget at " + std::to_string(reg(UC_X86_REG_CS)) + ":" + std::to_string(reg(UC_X86_REG_IP))};
  }
  auto input(unsigned int port, unsigned int size)->uint32_t {
    if(boot) {
      ++reads;
      memory_word(0x46c, static_cast<uint16_t>(reads / 100));
      chip.advance(0.01);
    }
    if(!boot && (port == 0x344 || port == 0x345)) {
      // UltraMID polls voice ramps synchronously during all-notes-off
      chip.sample();
      if(auto const irq{chip.advance(1000.0 / 44100)}) pending_irq = irq;
    }
    if(port == 0x40 || port == 0x42) return (65535 - reads) & 255;
    if(port == 0x61) return (reads & 1) * 0x20;
    if(port >= 0x240 && port <= 0x34f) return chip.read(port, size);
    return ports.at(port);
  }
  void output(unsigned int port, unsigned int size, unsigned int value) {
    ports.at(port) = static_cast<uint8_t>(value);
    if(port == 0x0c) dma_flip = 0;
    if(port == 0x82) dma_page = static_cast<uint8_t>(value);
    if(port == 6 || port == 7) {
      auto &target{port == 6 ? dma_address : dma_length};
      target = static_cast<uint16_t>((target & ~(255 << (dma_flip * 8))) | (value << (dma_flip * 8)));
      dma_flip ^= 1;
      chip.dma_count(dma_length);
    }
    if(port >= 0x240 && port <= 0x34f) chip.write(port, size, value);
  }
  void hook_failure(std::exception const &error) {
    failure = error.what();
    uc_emu_stop(cpu);
  }
  void initialise() {
    check(uc_open(UC_ARCH_X86, UC_MODE_16, &cpu));
    check(uc_mem_map(cpu, 0, 0x200000, UC_PROT_ALL));
    auto const executable{read_file(std::filesystem::exists(directory / "ULTRAMID.EXE") ? directory / "ULTRAMID.EXE" : directory.parent_path() / "ULTRAMID.EXE")};
    if(executable.size() < 28 || word(executable, 0) != 0x5a4d) throw std::runtime_error{"UltraMID is not a DOS executable"};
    size_t const header{static_cast<size_t>(word(executable, 8)) * 16};
    if(header >= executable.size()) throw std::runtime_error{"Invalid UltraMID executable header"};
    check(uc_mem_write(cpu, 0x10100, executable.data() + header, executable.size() - header));
    for(unsigned int i{}; i < word(executable, 6); ++i) {
      auto const position{word(executable, 24) + i * 4};
      uint32_t const address{static_cast<uint32_t>(word(executable, position + 2) + 0x1010) * 16 + word(executable, position)};
      memory_word(address, static_cast<uint16_t>(memory_word(address) + 0x1010));
    }
    for(unsigned int i{}; i < 256; ++i) memory_word(i * 4 + 2, i >= 0x78 && i <= 0x7f ? 0 : 0xf000);
    uint8_t const iret{0xcf};
    check(uc_mem_write(cpu, 0xf0000, &iret, 1));
    memory_word(0x410, 0x21);
    memory_word(0x413, 640);
    memory_word(0x10000, 0x20cd);
    memory_word(0x10002, 0x9000);
    memory_word(0x1002c, 0x800);
    constexpr char environment[]{"ULTRASND=240,3,3,5,5\0ULTRADIR=C:\\ULTRASND\0PATH=C:\\ULTRASND\0\0\1\0C:\\ULTRASND\\ULTRAMID.EXE"};
    check(uc_mem_write(cpu, 0x8000, environment, sizeof(environment)));
    constexpr std::array<uint8_t,5> command{3, ' ', '-', 'c', '\r'};
    check(uc_mem_write(cpu, 0x10080, command.data(), command.size()));
    reg(UC_X86_REG_CS, static_cast<uint16_t>(0x1010 + word(executable, 22)));
    reg(UC_X86_REG_IP, word(executable, 20));
    reg(UC_X86_REG_SS, static_cast<uint16_t>(0x1010 + word(executable, 14)));
    reg(UC_X86_REG_SP, word(executable, 16));
    reg(UC_X86_REG_DS, 0x1000);
    reg(UC_X86_REG_ES, 0x1000);
    uc_hook hook{};
    auto const intr{+[](uc_engine *, uint32_t number, void *context) {
      auto &self{*static_cast<implementation *>(context)};
      try { self.interrupt(number); } catch(std::exception const &error) { self.hook_failure(error); }
    }};
    auto const in{+[](uc_engine *, uint32_t port, int size, void *context)->uint32_t {
      auto &self{*static_cast<implementation *>(context)};
      try { return self.input(port, static_cast<unsigned int>(size)); } catch(std::exception const &error) { self.hook_failure(error); return 0; }
    }};
    auto const out{+[](uc_engine *, uint32_t port, int size, uint32_t value, void *context) {
      auto &self{*static_cast<implementation *>(context)};
      try { self.output(port, static_cast<unsigned int>(size), value); } catch(std::exception const &error) { self.hook_failure(error); }
    }};
    check(uc_hook_add(cpu, &hook, UC_HOOK_INTR, reinterpret_cast<void *>(intr), this, 1, 0));
    check(uc_hook_add(cpu, &hook, UC_HOOK_INSN, reinterpret_cast<void *>(in), this, 1, 0, UC_X86_INS_IN));
    check(uc_hook_add(cpu, &hook, UC_HOOK_INSN, reinterpret_cast<void *>(out), this, 1, 0, UC_X86_INS_OUT));
    run(static_cast<uint32_t>(reg(UC_X86_REG_CS)) * 16 + reg(UC_X86_REG_IP), 0, 100000000);
    if(!resident) throw std::runtime_error{"UltraMID did not finish initialisation"};
    for(unsigned int vector{0x78}; vector < 0x80; ++vector) {
      auto const segment{memory_word(vector * 4 + 2)};
      std::array<char,8> signature{};
      check(uc_mem_read(cpu, static_cast<uint32_t>(segment) * 16 + 0x103, signature.data(), signature.size()));
      if(std::string_view{signature.data(), signature.size()} == "ULTRAMID") {
        entry_segment = segment;
        entry_offset = memory_word(vector * 4);
        break;
      }
    }
    if(entry_segment == 0) throw std::runtime_error{"UltraMID entry point not installed"};
    boot = false;
    files.clear();
  }
  auto sample()->std::array<int16_t,2> {
    auto const output{chip.sample()};
    if(auto const raised{chip.advance(1000.0 / 44100)}) pending_irq = raised;
    unsigned int const irq{std::exchange(pending_irq, 0)};
    if(irq) {
      unsigned int const vector{irq - 1 + 8};
      call(memory_word(vector * 4 + 2), memory_word(vector * 4));
    }
    bios_phase += 1193180;
    if(bios_phase >= uint64_t{44100} * 65536) {
      bios_phase -= uint64_t{44100} * 65536;
      call(memory_word(8 * 4 + 2), memory_word(8 * 4));
    }
    return output;
  }
};

gus_synth::gus_synth(std::filesystem::path const &patch_directory, unsigned int const sample_rate)
  : state{std::make_unique<implementation>(patch_directory, sample_rate)} {
  if(sample_rate == 0) throw std::invalid_argument{"Gravis output sample rate must be positive"};
  if(std::filesystem::is_directory(patch_directory / "MIDI")) state->directory /= "MIDI";
  state->initialise();
}
gus_synth::~gus_synth() = default;

void gus_synth::send(midi_message const message) noexcept {
  state->call(state->entry_segment, state->entry_offset, 0x10, message.status);
  state->call(state->entry_segment, state->entry_offset, 0x10, message.first);
  if((message.status & 0xf0) != 0xc0 && (message.status & 0xf0) != 0xd0)
    state->call(state->entry_segment, state->entry_offset, 0x10, message.second);
}
void gus_synth::reset() noexcept {
  for(uint8_t channel{}; channel < 16; ++channel) {
    send({static_cast<uint8_t>(0xb0 | channel), 123, 0});
  }
}
void gus_synth::render(std::span<float> const stereo) noexcept {
  if(!state->primed) {
    state->current = state->sample();
    state->previous = state->current;
    state->primed = true;
  }
  for(size_t i{}; i + 1 < stereo.size(); i += 2) {
    while(state->phase >= state->rate) {
      state->phase -= state->rate;
      state->previous = state->current;
      state->current = state->sample();
    }
    float const fraction{static_cast<float>(state->phase) / static_cast<float>(state->rate)};
    for(size_t channel{}; channel < 2; ++channel)
      stereo[i + channel] = (state->previous[channel] + fraction * (state->current[channel] - state->previous[channel])) / 32768.0f;
    state->phase += 44100;
  }
}
} // namespace darker::audio
