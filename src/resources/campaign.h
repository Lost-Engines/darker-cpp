#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include "resources/archive_set.h"
#include "resources/scenario.h"

namespace darker::resources {

struct campaign_selection {
  resource_id resource;
  size_t record;
};

campaign_selection select_campaign_stage(uint8_t stage);

class campaign_resources {
private:
  archive_set const &archives;
  std::array<std::optional<scenario_resource>,15> scenarios;

public:
  explicit campaign_resources(archive_set const &archives);
  scenario_resource const &scenario(uint8_t stage);
};

} // namespace darker::resources
