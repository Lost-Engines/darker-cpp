#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstddef>
#include <span>
#include <string>
#include "resources/archive_set.h"
#include "resources/decoder.h"

namespace {

template<std::size_t Size>
std::string decode(std::array<unsigned char, Size> const &input, std::size_t const limit = 8'000'000) {
  /// Convert small independently constructed test streams to printable output
  auto const output{darker::resources::decompress(std::as_bytes(std::span{input}), limit)};
  return {reinterpret_cast<char const*>(output.data()), output.size()};
}

} // anonymous namespace

TEST_CASE("Resource sentinel and little-endian control word decode two plain literals") {
  std::array<unsigned char, 5> const input{'A', 'B', 0x00, 0xf0, 0x00};
  REQUIRE(decode(input) == "AB");
  REQUIRE(decode(input, 2) == "AB");
  REQUIRE_THROWS(decode(input, 1));
}

TEST_CASE("Overlapping short matches repeat newly produced bytes") {
  std::array<unsigned char, 6> const input{'A', 'B', 0x00, 0xef, 0x00, 0x00};
  REQUIRE(decode(input) == "ABBB");
  REQUIRE_THROWS(decode(input, 3));
}

TEST_CASE("Literal word runs are copied without executable XOR transformation") {
  // control bits: 10010, four zero count bits, then 1111 terminator
  std::array<unsigned char, 17> const input{'A', 'B', 0x78, 0x90, '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'x', 'y', 0x00};
  REQUIRE(decode(input) == "AB0123456789xy");
}

TEST_CASE("Truncation, invalid distance and trailing bytes are rejected") {
  std::array<unsigned char, 6> const valid{'A', 'B', 0x00, 0xef, 0x00, 0x00};
  auto const bytes{std::as_bytes(std::span{valid})};
  for(std::size_t size{0}; size != bytes.size(); ++size) {
    REQUIRE_THROWS(darker::resources::decompress(bytes.first(size)));
  }
  std::array<unsigned char, 6> const invalid_distance{'A', 'B', 0x00, 0xef, 0x02, 0x00};
  REQUIRE_THROWS(decode(invalid_distance));
  std::array<unsigned char, 6> const trailing{'A', 'B', 0x00, 0xf0, 0x00, 0x01};
  REQUIRE_THROWS(decode(trailing));
}

TEST_CASE("Supported resource directory is contiguous within each archive") {
  auto const directory{darker::resources::resource_directory()};
  REQUIRE(directory.size() == 164);
  std::array<unsigned int, 5> const counts{79, 10, 14, 45, 16};
  std::array<std::size_t, 5> const sizes{1'195'478, 1'337'498, 1'453'028, 1'402'901, 134'178};
  std::size_t entry_index{0};
  for(unsigned int archive{0}; archive != counts.size(); ++archive) {
    std::size_t offset{0};
    for(unsigned int slot{0}; slot != counts[archive]; ++slot) {
      auto const &entry{directory[entry_index++]};
      REQUIRE(entry.id.archive == archive);
      REQUIRE(entry.id.slot == slot);
      REQUIRE(entry.offset == offset);
      REQUIRE(entry.compressed_size > 0);
      offset += entry.compressed_size;
    }
    REQUIRE(offset == sizes[archive]);
  }
}
