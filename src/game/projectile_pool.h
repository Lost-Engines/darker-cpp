#pragma once

#include <array>
#include <cstdint>
#include <span>
#include "game/object_definition.h"
#include "game/object_list.h"
#include "game/projectile_placement.h"
#include "maths/world_coordinates.h"

namespace darker::game {

struct projectile_angular_rates {
  std::uint16_t reserved{}; // native word retained when a pool record is reused
  std::uint16_t pitch{};
  std::uint16_t turn{}; // heading response for direct homing, bank response for map homing
};

struct projectile {
  std::uint16_t native_id{0};
  projectile *next{nullptr};
  projectile *previous{nullptr};
  object_parameters parameters;
  object_pose placement;
  maths::world_position previous_position{};
  projectile_angular_rates angular_motion{};
  std::uint8_t flags{0};
  std::uint8_t lifecycle{0};
  std::uint8_t fade{0};
  std::uint16_t inherited_roll{0};
  std::uint16_t deadline{0};
  // Native target encoding pending world-object/cell target resolution.
  std::uint16_t target_token{0xffff};
};

struct projectile_launch {
  object_definition const &definition;
  launch_emitter const &emitter;
  std::uint16_t model_token{0};
  std::uint16_t clock{0};
  std::uint16_t lifetime{0};
  std::uint16_t target_token{0xffff};
};

enum class projectile_list { player, hostile };

class projectile_pool {
public:
  static std::size_t constexpr capacity{12};

  explicit projectile_pool(projectile_list category = projectile_list::player);
  projectile_pool(projectile_pool const &) = delete;
  projectile_pool &operator=(projectile_pool const &) = delete;
  projectile_pool(projectile_pool &&) = delete;
  projectile_pool &operator=(projectile_pool &&) = delete;

  projectile *launch(projectile_launch request);
  projectile *recycle(projectile &record);
  projectile *unlink(projectile &record);
  projectile *resolve(std::uint16_t native_id) noexcept;
  object_list<projectile> const &objects() const noexcept;
  std::span<projectile const> records() const noexcept;

private:
  std::array<projectile, capacity> storage;
  size_t count;
  uint16_t first_id;
  object_list<projectile> list;
};

} // namespace darker::game
