#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <type_traits>

namespace framework::render {

template<unsigned int rows, unsigned int columns> struct basic_indexed_framebuffer;

// Borrowed pixels, with independent visible width and storage pitch. Views never own or resize storage.
template<class Byte>
class basic_indexed_surface {
  static_assert(std::is_same_v<std::remove_const_t<Byte>, uint8_t>);

public:
  std::span<Byte> const pixels;
  int const width;
  int const height;
  size_t const stride;

  basic_indexed_surface(std::span<Byte> storage, int columns, int rows, size_t row_stride)
    : pixels{storage}, width{columns}, height{rows}, stride{row_stride} {
    /// Validate without multiplying untrusted dimensions; the final row needs no trailing padding
    if(width < 0 || height < 0 || stride < static_cast<size_t>(width)
      || (height != 0 && (pixels.size() < static_cast<size_t>(width)
        || (stride != 0 && static_cast<size_t>(height - 1) > (pixels.size() - static_cast<size_t>(width)) / stride)))) {
      throw std::invalid_argument{"Indexed surface dimensions exceed its storage"};
    }
  }

  template<unsigned int rows, unsigned int columns>
  basic_indexed_surface(basic_indexed_framebuffer<rows, columns> &frame) noexcept
    : pixels{frame.pixels}, width{columns}, height{rows}, stride{columns} {
    /// Borrow a fixed framebuffer without copying its pixels
  }

  template<unsigned int rows, unsigned int columns>
  basic_indexed_surface(basic_indexed_framebuffer<rows, columns> const &frame) noexcept requires std::is_const_v<Byte>
    : pixels{frame.pixels}, width{columns}, height{rows}, stride{columns} {
    /// Read-only borrowing retains the framebuffer's constness
  }

  template<unsigned int rows, unsigned int columns>
  basic_indexed_surface(basic_indexed_framebuffer<rows, columns> const &&) = delete;

  template<class Other>
  basic_indexed_surface(basic_indexed_surface<Other> const &source) noexcept requires (std::is_const_v<Byte> && std::is_same_v<Other, uint8_t>)
    : pixels{source.pixels}, width{source.width}, height{source.height}, stride{source.stride} {
    /// A writable view can be passed to readers without granting write access
  }

  std::span<Byte> row(int y) const noexcept {
    /// Drawing callers clip coordinates before accessing rows; exclude storage padding
    return pixels.subspan(static_cast<size_t>(y) * stride, static_cast<size_t>(width));
  }
};

using indexed_surface = basic_indexed_surface<uint8_t>;
using const_indexed_surface = basic_indexed_surface<uint8_t const>;

} // namespace framework::render
