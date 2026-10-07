#include "resources/campaign.h"
#include <stdexcept>

namespace darker::resources {

campaign_selection select_campaign_stage(uint8_t const stage) {
  /// BB12 converts the one-based saved stage into an archive-04 resource and an eight-record index
  if(stage == 0 || stage > 120) throw std::out_of_range{"Campaign stage is outside the normal scenario resources"};
  auto const index{static_cast<unsigned int>(stage - 1)};
  return {.resource{4,index >> 3}, .record{index & 7}};
}

campaign_resources::campaign_resources(archive_set const &archives) : archives{archives} {
  /// Retain each loaded resource at a stable address while briefings and game scripts borrow its bytes
}

scenario_resource const &campaign_resources::scenario(uint8_t const stage) {
  /// Load a campaign resource once without invalidating earlier presentation or script spans
  auto const selection{select_campaign_stage(stage)};
  auto &resource{scenarios[selection.resource.slot]};
  if(!resource) resource.emplace(archives.load(selection.resource));
  if(selection.record >= resource->records().size()) throw std::out_of_range{"Campaign stage has no scenario record"};
  return *resource;
}

scenario_resource const &campaign_resources::supplementary() {
  /// BB3D retains archive 04/15 for blackout and supply-pad contexts independently of normal campaign stages
  if(!shared_scenarios) shared_scenarios.emplace(archives.load({4,15}));
  return *shared_scenarios;
}

} // namespace darker::resources
