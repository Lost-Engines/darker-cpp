#pragma once

#include <array>
#include <cstdint>
#include <span>
#include "game/object_definition.h"
#include "game/object_list.h"
#include "game/projectile_placement.h"

namespace darker::game {

struct projectile {
  projectile *next{nullptr};
  projectile *previous{nullptr};
  object_parameters parameters;
  projectile_placement placement;
  std::array<std::uint16_t, 3> angular_motion{};
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

class projectile_pool {
public:
  static std::size_t constexpr capacity{12};

  projectile_pool();
  projectile_pool(projectile_pool const &) = delete;
  projectile_pool &operator=(projectile_pool const &) = delete;
  projectile_pool(projectile_pool &&) = delete;
  projectile_pool &operator=(projectile_pool &&) = delete;

  projectile *launch(projectile_launch request);
  projectile *recycle(projectile &record);
  object_list<projectile> const &objects() const noexcept;
  std::span<projectile const> records() const noexcept;

private:
  std::array<projectile, capacity> storage;
  object_list<projectile> list;
};

} // namespace darker::game
