#pragma once

#include <cstddef>
#include <cstdint>

// Integer and calling-convention vocabulary required by the isolated DOSBox core.
using Bit8u = std::uint8_t;
using Bit8s = std::int8_t;
using Bit16u = std::uint16_t;
using Bit16s = std::int16_t;
using Bit32u = std::uint32_t;
using Bit32s = std::int32_t;
using Bitu = std::uintptr_t;
using Bits = std::intptr_t;
#define INLINE inline
#define DB_FASTCALL
#define GCC_UNLIKELY(condition) (condition)
