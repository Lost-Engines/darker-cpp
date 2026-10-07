#include <catch2/catch_test_macros.hpp>
#include "game/weapon_target.h"
#include "reference/weapon_target_samples.h"

TEST_CASE("Caero target retention matches native view-cone and target-kind tests", "[game][weapons]") {
  /// Compare retained and rejected locks including partial screen-coordinate writes
  for(auto const &s : darker::test_reference::weapon_target_samples) {
    CAPTURE(s);
    auto const word{[](int const value){ return static_cast<uint16_t>(value); }};
    auto const byte{[](int const value){ return static_cast<uint8_t>(value); }};
    darker::game::weapon_target lock{.token{word(s[11])}};
    auto const basis{darker::maths::make_view_basis({word(s[0]),word(s[1]),word(s[2])})};
    darker::game::project_caero_target(lock,{word(s[3]),word(s[4]),word(s[5])},
      {word(s[6]),word(s[7]),word(s[8])},word(s[9]),basis,byte(s[10]),byte(s[12]),byte(s[13]));
    CHECK(lock.token == s[14]);
    CHECK(lock.spread == s[15]);
    CHECK(static_cast<uint16_t>(lock.horizontal) == s[16]);
    CHECK(static_cast<uint16_t>(lock.vertical) == s[17]);
    CHECK(lock.distance == s[18]);
  }
}

TEST_CASE("Target acquisition rays match native fixed-point endpoints", "[game][weapons]") {
  /// Include wrapping positions and all quadrants without approximating the ray with floating-point trigonometry
  for(auto const &s : darker::test_reference::target_ray_samples) {
    CAPTURE(s);
    auto const word{[](int const value){ return static_cast<uint16_t>(value); }};
    darker::game::object_pose const player{.position{word(s[0]),word(s[1]),word(s[2])},.angles{word(s[3]),word(s[4]),0}};
    CHECK(darker::game::target_ray_end(player) == std::array<uint16_t,3>{word(s[5]),word(s[6]),word(s[7])});
  }
}
