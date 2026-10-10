#pragma once

#include <array>
#include <cstdint>
#include <span>
#include "game/object_definition.h"
#include "game/native_object_layout.h"
#include "game/object_list.h"
#include "game/projectile_placement.h"
#include "game/time.h"
#include "maths/world_coordinates.h"

namespace darker::game {

struct projectile_angular_rates {
  uint16_t reserved{};                                                         // native word retained when a pool record is reused
  uint16_t pitch{};
  uint16_t turn{};                                                             // heading response for direct homing, bank response for map homing
};

struct projectile {
  uint16_t native_id{0};
  projectile *next{nullptr};
  projectile *previous{nullptr};
  object_parameters parameters;
  object_pose placement;
  maths::world_position previous_position{};
  projectile_angular_rates angular_motion{};
  uint8_t flags{0};
  uint8_t lifecycle{0};
  uint8_t fade{0};
  uint16_t inherited_roll{0};
  clock_tick deadline{0};
  // native target encoding pending world-object/cell target resolution
  uint16_t target_token{0xffff};
};

struct projectile_launch {
  object_definition const &definition;
  launch_emitter const &emitter;
  uint16_t model_token{0};
  clock_tick clock{0};
  game_duration lifetime{0};
  uint16_t target_token{0xffff};
};

enum class projectile_list { player, hostile };

class projectile_pool {
public:
  static unsigned int constexpr capacity{projectile_limits::player_capacity};

  explicit projectile_pool(projectile_list category = projectile_list::player);
  projectile_pool(projectile_pool const&) = delete;
  projectile_pool &operator=(projectile_pool const&) = delete;
  projectile_pool(projectile_pool &&) = delete;
  projectile_pool &operator=(projectile_pool &&) = delete;

  projectile *launch(projectile_launch request);
  projectile *recycle(projectile &record);
  projectile *unlink(projectile &record);
  projectile *resolve(uint16_t native_id) noexcept;
  object_list<projectile> const &objects() const noexcept;
  std::span<projectile const> records() const noexcept;

private:
  std::array<projectile, capacity> storage;
  size_t count;
  uint16_t first_id;
  object_list<projectile> list;
};

} // namespace darker::game
