#pragma once

// included inside the upstream core namespace, after its register definitions
struct device_state {
  GFGus registers{};
  Bit8u ram[1024 * 1024]{};
  size_t ram_size{sizeof(ram)};
  GUSChannels *channels[32]{nullptr};
  GUSChannels *selected{nullptr};
  Bit8u adlib{};
  Bit32s amplification{512};
  Bit16u volumes[4096]{};
  Bit32u panning[16]{};
  MixerChannel mixer;
  MixerChannel *mixer_pointer{&mixer};
  Bit32s mix[4]{};
  DmaChannel dma;
  std::function<void(std::span<std::byte>)> dma_reader;
  struct event {
    void (*callback)(Bitu){
  };
    double due{0.0};
    Bitu argument{};
  } events[2];
  double time{0.0};
  unsigned int irq{0};
};
static thread_local device_state *context{nullptr};
#define myGUS (context->registers)
#define GUSRam (context->ram)
#define guschan (context->channels)
#define curchan (context->selected)
#define adlib_commandreg (context->adlib)
#define AutoAmp (context->amplification)
#define vol16bit (context->volumes)
#define pantable (context->panning)
#define gus_chan (context->mixer_pointer)
#define MixTemp (context->mix)
