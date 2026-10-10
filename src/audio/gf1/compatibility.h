#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include "audio/gf1/device.h"

namespace darker::audio::gf1_detail {
using Bit8u = uint8_t;
using Bit8s = int8_t;
using Bit16u = uint16_t;
using Bit16s = int16_t;
using Bit32u = uint32_t;
using Bit32s = int32_t;
using Bitu = uintptr_t;
using Bits = intptr_t;
#define INLINE inline
#define LOG_MSG(...) ((void)0)
struct MixerChannel {
  void Enable(bool) {
    // the host owns stream lifetime
  }
  void AddSamples_s16(Bitu, Bit16s*) {
    // the adapter reads the core mixing buffer directly
  }
};
enum DMAEvent { DMA_UNMASKED };
struct DmaChannel {
  unsigned int currcnt{}, DMA16{};
  auto Read(Bitu count, void *target)->Bitu;
  void Write(Bitu, void*);
  void Register_Callback(void (*callback)(DmaChannel*, DMAEvent));
};
auto GetDMAChannel(unsigned int)->DmaChannel *;
void PIC_ActivateIRQ(Bitu irq);
void PIC_AddEvent(void (*callback)(Bitu), float delay, Bitu argument);
} // namespace darker::audio::gf1_detail
