#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

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
  void configure_music(std::span<std::byte const> driver, std::array<std::vector<std::byte>,6> songs);
  void select_music(int group) noexcept;
  bool publish(fm_frame const &frame) noexcept;
  void render(std::span<float> stereo) noexcept;
};

} // namespace darker::audio
