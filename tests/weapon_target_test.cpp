#include <catch2/catch_test_macros.hpp>
#include "game/weapon_target.h"
#include "maths/world_coordinates.h"
#include "reference/skimma_target_samples.h"
#include "reference/weapon_target_samples.h"

TEST_CASE("Caero target retention matches native view-cone and target-kind tests", "[game][weapons]") {
  /// Compare retained and rejected locks including partial screen-coordinate writes
  for(auto const &s : darker::test_reference::weapon_target_samples) {
    CAPTURE(s);
    auto const word{[](int const value){
      return static_cast<uint16_t>(value);
    }};
    auto const byte{[](int const value){
      return static_cast<uint8_t>(value);
    }};
    darker::game::weapon_target lock{
      .token{word(s[11])}
    };
    auto const basis{darker::maths::make_view_basis({word(s[0]), word(s[1]), word(s[2])})};
    lock.project_caero({word(s[3]), word(s[4]), word(s[5])},
      {word(s[6]), word(s[7]), word(s[8])}, word(s[9]), basis, byte(s[10]), byte(s[12]), byte(s[13]));
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
    auto const word{[](int const value){
      return static_cast<uint16_t>(value);
    }};
    darker::game::object_pose const player{
      .position{
        .column{word(s[0])},
        .row{word(s[1])},
        .height{word(s[2])}
      },
      .angles{
        .heading{word(s[3])},
        .pitch{word(s[4])},
        .roll{0}
      }
    };
    CHECK(darker::game::target_ray_end(player) == darker::maths::world_position{
      .column{word(s[5])},
      .row{word(s[6])},
      .height{word(s[7])}
    });
  }
}

TEST_CASE("Skimma target retention matches native rings and target restrictions", "[game][weapons]") {
  /// Compare original reload, enable, damage-link and wider targeting-cone decisions
  for(auto const &s : darker::test_reference::skimma_target_samples) {
    CAPTURE(s);
    auto const word{[](int const value){
      return static_cast<uint16_t>(value);
    }};
    darker::game::weapon_target lock{
      .token{word(s[11])}
    };
    auto const basis{darker::maths::make_view_basis({word(s[0]), word(s[1]), word(s[2])})};
    lock.project_skimma({word(s[3]), word(s[4]), word(s[5])},
      {word(s[6]), word(s[7]), word(s[8])}, word(s[9]), basis, static_cast<uint8_t>(s[10]), s[14] & 1, s[15], s[16]);
    CHECK(lock.token == s[17]);
    CHECK(lock.spread == s[18]);
    CHECK(static_cast<uint16_t>(lock.horizontal) == s[19]);
    CHECK(static_cast<uint16_t>(lock.vertical) == s[20]);
    CHECK(lock.distance == s[21]);
  }
}

TEST_CASE("Target encoding retains the distinct native firing boundary", "[game][weapons]") {
  using darker::game::target_reference;
  target_reference const ground{0x067f};
  CHECK(ground.is_ground_encoded());
  CHECK(ground.cell().column() == 127);
  CHECK(ground.cell().row() == 6);
  CHECK(ground.cell().index() == 895);
  CHECK_FALSE(ground.permits_air_weapon());

  target_reference const boundary{0x7fff};
  CHECK(boundary.is_ground_encoded());
  CHECK(boundary.permits_air_weapon());
  CHECK_FALSE(target_reference{0x7ffe}.permits_air_weapon());
  CHECK(target_reference{0x8000}.is_object_encoded());
  CHECK(target_reference{0xfffe}.permits_air_weapon());
  CHECK(target_reference{}.is_none());
  CHECK(target_reference{}.is_object_encoded());
  CHECK_FALSE(target_reference{}.permits_air_weapon());

  // packed tokens discard column bit seven; separate coordinate indexing must not do so
  CHECK(target_reference{0x0681}.cell().column() == 1);
  CHECK(darker::game::city_cell_index(129, 6) == 897);
}
