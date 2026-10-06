#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace darker::audio {

struct fm_write {
  std::uint8_t address;
  std::uint8_t value;
};

struct fm_program {
  std::array<fm_write, 16> writes{};
  std::size_t count{0};

  std::span<fm_write const> view() const noexcept { return {writes.data(), count}; }
};

struct fm_voice {
  std::uint8_t patch{255};
  std::uint8_t block{31};
};

class fm_driver {
private:
  std::array<fm_voice, 9> voices{};

public:
  fm_program program(std::uint8_t channel, std::uint8_t patch, std::uint16_t pitch, std::uint16_t level, bool key_on, bool retrigger = false);
  fm_program stop(std::uint8_t channel);
};

} // namespace darker::audio
