#include <catch2/catch_test_macros.hpp>
#include <array>
#include <vector>
#include "game/vehicle_route.h"
#include "reference/vehicle_route_samples.h"

TEST_CASE("Ground vehicle routes match native movement through turns, branches and clock wrapping") {
  /// Compare every retained field across complete flatbed routes and repeated synthetic route loops
  std::array<std::vector<uint8_t>,2> const programs{{
    {0x50,0x51,0x03,0x02,0x41,0x03,0x31,0x02,0x41,0x03,0x11,0x03,0x10,0x11,0x08},
    {0x10,0x21,0x02,0x81,0x03,0x11,0xff,0xf9},
  }};
  for(auto const &sample : darker::test_reference::vehicle_route_samples) {
    CAPTURE(sample.program,sample.heading,sample.step);
    darker::game::vehicle_route route;
    darker::game::object_pose pose{.position{14720,20608,0},.angles{static_cast<uint16_t>(sample.heading),0,0}};
    uint8_t flags{0};
    uint64_t hash{0xcbf29ce484222325};
    for(unsigned int call{0}; call < sample.calls; ++call) {
      REQUIRE_FALSE(route.removed);
      darker::game::advance_vehicle_route(route,pose,flags,std::as_bytes(std::span{programs[sample.program]}),
        static_cast<uint16_t>(call * sample.step),37);
      std::array<uint64_t,15> const words{pose.position[0],pose.position[1],pose.position[2],
        pose.fractions[0],pose.fractions[1],pose.fractions[2],pose.angles[0],pose.angles[1],pose.angles[2],
        pose.speed,flags,route.origin,route.cursor,route.command,static_cast<uint64_t>(route.removed)};
      for(auto const word : words) hash = (hash ^ word) * 0x100000001b3;
    }
    REQUIRE(hash == sample.fingerprint);
    REQUIRE(route.removed == (sample.program == 0));
  }
}
