#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <stdexcept>
#include <string_view>
#include "resources/save_file.h"

TEST_CASE("Save checksum matches CRC-16/XMODEM", "[resources][save]") {
  /// Use the independent standard check vector, not an encoder/decoder roundtrip
  std::string_view const reference{"123456789"};
  CHECK(darker::resources::save_checksum(std::as_bytes(std::span{reference})) == 0x31c3);
}

TEST_CASE("Save fields occupy original DOS offsets and retain unknown bytes", "[resources][save]") {
  /// A patterned payload has CRC 7584 verified with native A110/0364 and Python crc_hqx
  std::array<std::byte, 6600> bytes{};
  for(size_t i{0}; i < 6598; ++i) bytes[i] = static_cast<std::byte>((i * 37 + 11) & 255);
  bytes[6598] = std::byte{0x84};
  bytes[6599] = std::byte{0x75};
  auto save{darker::resources::decode_save(bytes)};
  CHECK(darker::resources::encode_save(save) == bytes);
  CHECK(save.pilots[1].stage == std::to_integer<uint8_t>(bytes[1649]));
  CHECK(save.pilots[0].weapons == 0x8661);
  CHECK(save.pilots[0].return_site == 0xd0ab);
  CHECK(save.pilots[0].delphi.front() == bytes[0x22]);
  CHECK(save.pilots[0].halon.front() == bytes[0x3db]);
  CHECK(save.pilots[0].reserved.front() == bytes[0x5af]);
  CHECK(save.pilots[0].reserved.back() == bytes[0x670]);
  CHECK(save.trailer[0] == bytes[6596]);
  save.pilots[2].set_name("Tolly");
  auto const edited{darker::resources::encode_save(save)};
  CHECK(std::equal(bytes.begin(), bytes.begin() + 3299, edited.begin()));
  CHECK(std::equal(bytes.begin() + 3328, bytes.begin() + 6598, edited.begin() + 3328));
  CHECK(darker::resources::decode_save(edited).pilots[2].display_name() == "Tolly");
}

TEST_CASE("Save corruption and invalid lengths cannot silently reset progress", "[resources][save]") {
  /// Flips in records, opaque regions, trailer and checksum must all be rejected
  auto const valid{darker::resources::encode_save({})};
  for(size_t const offset : {0u, 1u, 30u, 1649u, 6595u, 6596u, 6598u, 6599u}) {
    auto damaged{valid};
    damaged[offset] ^= std::byte{1};
    CHECK_THROWS_AS(darker::resources::decode_save(damaged), std::invalid_argument);
  }
  for(size_t const length : {0u, 1649u, 6598u, 6599u}) {
    CHECK_THROWS_AS(darker::resources::decode_save(std::span{valid}.first(length)), std::invalid_argument);
  }
  std::array<std::byte, 6601> oversized{};
  CHECK_THROWS_AS(darker::resources::decode_save(oversized), std::invalid_argument);
  darker::resources::pilot_record pilot;
  pilot.name.fill('A');
  CHECK(pilot.display_name().size() == 29);
  CHECK_THROWS_AS(pilot.set_name(std::string(29, 'B')), std::invalid_argument);
}
