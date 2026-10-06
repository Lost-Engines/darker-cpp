#include "decoder.h"
#include <format>
#include <stdexcept>
#include <utility>

namespace darker::resources {
namespace {

class decoder {
private:
  std::span<std::byte const> input;
  std::size_t output_limit;
  std::size_t position{0};
  unsigned int bits{0x4000};
  std::vector<std::byte> output;

  unsigned int byte() {
    /// Read one byte from the interleaved literal/control stream
    if(position == input.size()) throw std::runtime_error{"truncated resource stream"};
    return std::to_integer<unsigned int>(input[position++]);
  }

  unsigned int bit() {
    /// Match the native MSB-first word reader, including its initial sentinel
    unsigned int carry{bits >> 15};
    bits = (bits << 1) & 0xffffu;
    if(bits == 0) {
      unsigned int const low{byte()};
      unsigned int const word{low | (byte() << 8)};
      carry = word >> 15;
      bits = ((word << 1) | 1u) & 0xffffu;
    }
    return carry;
  }

  void append(unsigned int const value) {
    /// Bound expansion before modifying the owning output buffer
    if(output.size() == output_limit) throw std::runtime_error{"resource output limit exceeded"};
    output.push_back(static_cast<std::byte>(value));
  }

  void copy(unsigned int const length, unsigned int const distance) {
    /// Copy forwards one byte at a time so overlapping matches repeat their own output
    if(distance == 0 || distance > output.size()) {
      throw std::runtime_error{std::format("invalid resource back-reference at compressed offset {}", position)};
    }
    for(unsigned int i{0}; i != length; ++i) {
      append(std::to_integer<unsigned int>(output[output.size() - distance]));
    }
  }

public:
  decoder(std::span<std::byte const> const input, std::size_t const output_limit) :
    input{input},
    output_limit{output_limit} {
  }

  std::vector<std::byte> run() {
    /// Decode original archive literals, runs and matches until the explicit end token
    append(byte());
    for(;;) {
      if(bit() == 0) {
        append(byte());
        continue;
      }
      unsigned int length{3};
      unsigned int high{0};
      if(bit() == 0) {
        length = bit() * 2;
        length += bit();
        if(length != 0) {
          length = (length - 1) * 2 + bit();
          if(length == 0) {
            unsigned int count{0};
            for(unsigned int i{0}; i != 4; ++i) count = count * 2 + bit();
            count = (count * 2 + 6) * 2;
            for(unsigned int i{0}; i != count; ++i) append(byte());
            continue;
          }
        }
        length += 4;
      } else if(bit() != 0) {
        length = 2;
        if(bit() != 0) {
          unsigned int const extra{byte()};
          if(extra == 0) break;
          length = extra + 9;
        } else {
          copy(length, byte() + 1);
          continue;
        }
      }
      if(bit() != 0) {
        high = bit();
        if(bit() != 0) {
          high = (high * 2 + bit()) | 4u;
          if(bit() == 0) high = high * 2 + bit();
        } else if(high == 0) {
          high = 2 + bit();
        }
      }
      copy(length, (high << 8) + byte() + 1);
    }
    if(position != input.size()) throw std::runtime_error{"trailing bytes after resource terminator"};
    return std::move(output);
  }
};

} // anonymous namespace

std::vector<std::byte> decompress(std::span<std::byte const> const input, std::size_t const output_limit) {
  /// Decode a bounded resource without retaining references to the compressed source
  return decoder{input, output_limit}.run();
}

} // namespace darker::resources
