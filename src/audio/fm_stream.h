#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <span>

namespace darker::audio {

struct fm_note {
  std::uint16_t pitch{0};
  std::uint16_t level{0};
  std::uint16_t generation{0};
  std::uint8_t patch{0};
  bool active{false};
};

using fm_frame = std::array<fm_note, 9>;

class fm_stream {
private:
  struct implementation;
  std::unique_ptr<implementation> state;

public:
  explicit fm_stream(unsigned int sample_rate);
  ~fm_stream();
  bool publish(fm_frame const &frame) noexcept;
  void render(std::span<float> stereo) noexcept;
};

} // namespace darker::audio
