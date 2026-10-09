#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>

namespace darker::audio {
namespace gf1_detail { struct device_state; }

class gf1_device {
private:
  std::unique_ptr<gf1_detail::device_state> state;

public:
  explicit gf1_device(std::function<void(std::span<std::byte>)> dma_read);
  ~gf1_device();
  auto read(unsigned int port, unsigned int size)->unsigned int;
  void write(unsigned int port, unsigned int size, unsigned int value);
  void dma_count(unsigned int count);
  auto advance(double milliseconds)->unsigned int;
  auto sample()->std::array<int16_t,2>;
};

} // namespace darker::audio
