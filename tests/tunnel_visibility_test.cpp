#include <catch2/catch_test_macros.hpp>
#include <array>
#include "graphics/tunnel_visibility.h"
#include "reference/tunnel_visibility_samples.h"

TEST_CASE("Underground visibility propagation matches native visit order and culling feedback") {
  /// Compare three successive frames on each map, preserving the native low state bit between scans
  for(unsigned int seed{0}; seed < darker::test_reference::tunnel_visibility_samples.size(); ++seed) {
    CAPTURE(seed);
    darker::game::city_map cells{};
    std::array<uint8_t,128*128> visibility{};
    for(unsigned int i{0}; i < cells.size(); ++i) {
      cells[i].type = static_cast<uint8_t>(seed % 4 == 0 || ((i*37) ^ (i >> 4) ^ (seed*17)) % 7 != 0);
      visibility[i] = static_cast<uint8_t>((i + seed) % 3 == 0);
    }
    uint64_t fingerprint{0xcbf29ce484222325};
    auto const add{[&](uint64_t const word){ fingerprint = (fingerprint ^ word) * 0x100000001b3; }};
    for(unsigned int frame{0}; frame < 3; ++frame) {
      auto const x{static_cast<uint8_t>(10 + (seed*17 + frame*3) % 106)};
      auto const y{static_cast<uint8_t>(10 + (seed*23 + frame*2) % 106)};
      darker::graphics::visit_tunnel_cells(cells,x,y,visibility,[&](uint16_t const index){
        bool const accepted{(index*13 + seed*19 + frame*3) % 11 >= seed % 7};
        add(index);
        add(accepted);
        return accepted;
      });
      for(auto const state : visibility) add(state);
    }
    REQUIRE(fingerprint == darker::test_reference::tunnel_visibility_samples[seed]);
  }
}
