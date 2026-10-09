// Included after the fetched GF1 core; only this adapter sees its internal types.
auto DmaChannel::Read(Bitu count, void *target)->Bitu {
  context->dma_reader({static_cast<std::byte *>(target), count});
  return count;
}
void DmaChannel::Write(Bitu, void *) {
  throw std::runtime_error{"UltraMID requested unsupported recording DMA"};
}
void DmaChannel::Register_Callback(void (*callback)(DmaChannel *, DMAEvent)) {
  if(callback) callback(this, DMA_UNMASKED);
}
auto GetDMAChannel(unsigned int)->DmaChannel * {
  return &context->dma;
}
void PIC_ActivateIRQ(Bitu irq) {
  context->irq = static_cast<unsigned int>(irq) + 1;
}
void PIC_AddEvent(void (*callback)(Bitu), float delay, Bitu argument) {
  context->events[argument] = {callback, context->time + delay, argument};
}
} // namespace darker::audio::gf1_detail

namespace darker::audio {
gf1_device::gf1_device(std::function<void(std::span<std::byte>)> dma_read)
  : state{std::make_unique<gf1_detail::device_state>()} {
  auto &core{*state};
  gf1_detail::context = &core;
  core.dma_reader = std::move(dma_read);
  core.registers.rate = 44100;
  core.registers.portbase = 0x40;
  core.registers.dma1 = core.registers.dma2 = 3;
  core.registers.irq1 = core.registers.irq2 = 5;
  gf1_detail::MakeTables();
  for(uint8_t i{}; i < 32; ++i) core.channels[i] = new gf1_detail::GUSChannels{i};
  core.registers.gRegData = 1;
  gf1_detail::GUSReset();
  core.registers.gRegData = 0;
}
gf1_device::~gf1_device() {
  for(auto *channel : state->channels) delete channel;
}
auto gf1_device::read(unsigned int port, unsigned int size)->unsigned int {
  gf1_detail::context = state.get();
  return static_cast<unsigned int>(gf1_detail::read_gus(port, size));
}
void gf1_device::write(unsigned int port, unsigned int size, unsigned int value) {
  gf1_detail::context = state.get();
  gf1_detail::write_gus(port, value, size);
}
void gf1_device::dma_count(unsigned int count) {
  state->dma.currcnt = count;
}
auto gf1_device::advance(double milliseconds)->unsigned int {
  gf1_detail::context = state.get();
  double const target{state->time + milliseconds};
  for(;;) {
    auto *next{&state->events[0]};
    if(!next->callback || (state->events[1].callback && state->events[1].due < next->due)) next = &state->events[1];
    if(!next->callback || next->due > target) break;
    auto const due{*next};
    *next = {};
    state->time = due.due;
    due.callback(due.argument);
  }
  state->time = target;
  return std::exchange(state->irq, 0);
}
auto gf1_device::sample()->std::array<int16_t,2> {
  gf1_detail::context = state.get();
  gf1_detail::GUS_CallBack(1);
  std::array<int16_t,2> output{};
  std::memcpy(output.data(), state->mix, sizeof(output));
  return output;
}
} // namespace darker::audio
