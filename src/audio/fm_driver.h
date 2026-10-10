#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace darker::audio {

struct fm_write {
  uint8_t address;
  uint8_t value;
};

struct fm_program {
  std::array<fm_write, 16> writes{};
  size_t count{0};

  std::span<fm_write const> view() const noexcept {
    return {writes.data(), count};
  }
};

struct fm_voice {
  uint8_t patch{255};
  uint8_t block{31};
};

class fm_driver {
private:
  std::array<fm_voice, 9> voices{};

public:
  fm_program program(uint8_t channel, uint8_t patch, uint16_t pitch, uint16_t level, bool key_on, bool retrigger = false);
  fm_program stop(uint8_t channel);
};

} // namespace darker::audio
