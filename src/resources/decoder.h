#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace darker::resources {

std::vector<std::byte> decompress(std::span<std::byte const> input, std::size_t output_limit = 8'000'000);

} // namespace darker::resources
