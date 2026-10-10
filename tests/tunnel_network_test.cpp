#include <catch2/catch_test_macros.hpp>
#include <array>
#include "game/tunnel_network.h"
#include "maths/world_coordinates.h"
#include "reference/tunnel_network_samples.h"

TEST_CASE("Tunnel route preparation and interpolation match native geometry helpers") {
  /// Verify the prepared route table and signed interpolation across all three edges and both traversal directions
  std::array<std::byte, 2000> source{};
  for(unsigned int tile{0}; tile < 200; ++tile) {
    source[tile * 10] = static_cast<std::byte>(tile & 3);
    for(unsigned int edge{0}; edge < 3; ++edge) {
      auto const first{(tile * 17 + edge * 29) & 127};
      source[tile * 10 + 1 + edge * 3] = static_cast<std::byte>(first);
      source[tile * 10 + 2 + edge * 3] = static_cast<std::byte>((first + edge * 13 + 17) & 127);
      source[tile * 10 + 3 + edge * 3] = static_cast<std::byte>((tile * 19 + edge * 71) & 255);
    }
  }
  darker::game::tunnel_network const network{source};
  auto const prepared{network.prepared_bytes()};
  for(size_t i{0}; i < prepared.size(); ++i) {
    CAPTURE(i);
    REQUIRE(std::to_integer<uint8_t>(prepared[i]) == darker::test_reference::prepared_tunnel_routes[i]);
  }
  for(auto const &sample : darker::test_reference::tunnel_point_samples) {
    CAPTURE(sample);
    auto const type{static_cast<uint8_t>(sample[0])}, route{static_cast<uint8_t>(sample[1])};
    REQUIRE(network.point(type, route, sample[2], sample[3]) == darker::maths::world_position{
      .column{sample[5]},
      .row{sample[6]},
      .height{sample[7]}
    });
    REQUIRE(network.direction(type, route, sample[4]) == sample[8]);
  }
}
