#include "audio/voice_allocation.h"
#include <algorithm>

namespace darker::audio {

fm_frame voice_allocation::allocate(std::span<sound_candidate const> const sources) {
  /// 3488 retains nine ascending candidates; 33F1 reuses voices through persistent active/free lists
  std::vector<sound_candidate> candidates;
  candidates.reserve(10);
  for(auto const &source : sources) {
    if(!source.note.active) continue;
    if(candidates.size() == 9 && source.note.level <= candidates.front().note.level) continue;
    auto const at{std::ranges::lower_bound(candidates, source.note.level, {}, [](auto const &value){
      return value.note.level;
    })};
    candidates.insert(at, source);
    if(candidates.size() > 9) candidates.erase(candidates.begin());
  }
  fm_frame frame{};
  std::vector<size_t> next;
  next.reserve(9);
  for(auto const channel : active) {
    auto const found{std::ranges::find(candidates, owners[channel], &sound_candidate::identity)};
    if(found == candidates.end()) {
      owners[channel] = 0;
      free.insert(free.begin(), channel);
    } else {
      if(source_generations[channel] != found->note.generation) ++generations[channel];
      source_generations[channel] = found->note.generation;
      frame[channel] = found->note;
      frame[channel].generation = generations[channel];
      next.insert(next.begin(), channel);
      candidates.erase(found);
    }
  }
  for(auto const &source : candidates) {
    auto const channel{free.front()};
    free.erase(free.begin());
    owners[channel] = source.identity;
    source_generations[channel] = source.note.generation;
    frame[channel] = source.note;
    frame[channel].generation = ++generations[channel];
    next.insert(next.begin(), channel);
  }
  active = std::move(next);
  return frame;
}

std::array<uint64_t, 9> const &voice_allocation::identities() const noexcept {
  /// Expose retained ownership for the original fixed-record continuation checks
  return owners;
}

} // namespace darker::audio
