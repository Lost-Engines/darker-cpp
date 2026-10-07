#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>
#include "audio/fm_stream.h"

namespace darker::audio {

struct sound_candidate {
  uint64_t identity;
  fm_note note;
};

class voice_allocation {
private:
  std::array<uint64_t,9> owners{};
  std::array<uint16_t,9> generations{};
  std::array<uint16_t,9> source_generations{};
  std::vector<size_t> active;
  std::vector<size_t> free{0,1,2,3,4,5,6,7,8};

public:
  fm_frame allocate(std::span<sound_candidate const> sources);
  std::array<uint64_t,9> const &identities() const noexcept;
};

} // namespace darker::audio
