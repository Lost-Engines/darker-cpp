#include "camera_target_check.h"
#include <stdexcept>
#include "game/camera_target.h"
#include "game/mission_combat.h"
#include "game/projectile_steering.h"

void check_camera_targets(darker::resources::archive_set const &archives) {
  /// Exercise F7's full city/object selection with retail collision models and native list order.
  using namespace darker::game;
  darker::resources::geometry_bank const bank{archives.load({0,30})};
  city_map cells{};
  object_pose const camera{.position{10000,11000,1000}};
  object_pose const player{.position{8000,11000,1000}};
  std::array<scenario_actor,3> actors;
  constexpr std::array categories{actor_category::ground,actor_category::stationary,actor_category::air};
  for(size_t i{0}; i < actors.size(); ++i) {
    actors[i].category = categories[i];
    actors[i].index = static_cast<uint8_t>(i+1);
    actors[i].pose.position = {10000,10000,1000};
    actors[i].parameters.model_token = bank.special_models()[25];
  }
  auto const extent{bank.header_at(bank.special_models()[25]).extent};
  for(auto const excluded : {uint8_t{0},uint8_t{3}}) {
    auto const picked{pick_camera_target(camera,player,extent,excluded,actors,cells,bank,0x20)};
    if(!picked || picked->actor != (excluded == 3 ? 2 : 3)) throw std::runtime_error{"F7 object selection lost native list order or exclusion"};
  }
  auto const own_craft{pick_camera_target(camera,actors[0].pose,extent,std::nullopt,{},cells,bank,0x20)};
  if(!own_craft || own_craft->actor != 0) throw std::runtime_error{"F7 failed to select the player from an external view"};
  if(pick_camera_target(camera,actors[0].pose,extent,0,{},cells,bank,0x20)) throw std::runtime_error{"F7 selected its excluded craft"};
  cells[20*128+20] = {.type{1}};
  object_pose const approaching{.position{20*256+128,22*256,200}};
  auto const beacon{pick_camera_target(approaching,approaching,extent,0,{},cells,bank,0x20)};
  if(!beacon || beacon->actor || beacon->anchor.position[0] != 20*256+128 || beacon->anchor.position[1] != 20*256+128)
    throw std::runtime_error{"F7 failed to place a camera over a struck beacon"};
  auto const model{bank.header_at(bank.city_model_offset(1,0,0x20))};
  if(beacon->anchor.position[2] != static_cast<uint16_t>(model.extent*5-model.height))
    throw std::runtime_error{"F7 building camera height differs from D089/25C3"};
  actors[2].flags = 0x20;
  actors[2].expiry = 0;
  mission_combat combat{{actors[2]}};
  combat.camera_actor = 3;
  player_flight flying;
  flying.pose() = player;
  combat.advance(flying, cells, bank,
        {.elapsed_ticks{1}, .frame_step{1}, .changes{0}},
        {},
        {});
  if(combat.camera_actor) throw std::runtime_error{"Removed F7 object retained a camera reference for pool reuse"};
}
