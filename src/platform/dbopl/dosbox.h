#pragma once

#include <cstddef>
#include <cstdint>

// integer and calling-convention vocabulary required by the isolated DOSBox core
using Bit8u = uint8_t;
using Bit8s = int8_t;
using Bit16u = uint16_t;
using Bit16s = int16_t;
using Bit32u = uint32_t;
using Bit32s = int32_t;
using Bitu = uintptr_t;
using Bits = intptr_t;
#define INLINE inline
#define DB_FASTCALL
#define GCC_UNLIKELY(condition) (condition)
