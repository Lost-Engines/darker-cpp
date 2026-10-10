#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>
#include "audio/fm_synth.h"
#include "audio/midi_music.h"
#include "audio/sc55_synth.h"

namespace darker::audio {

struct fm_note {
  std::uint16_t pitch{0};
  std::uint16_t level{0};
  std::uint16_t generation{0};
  std::uint8_t patch{0};
  bool active{false};
  std::array<uint8_t,2> attenuation{255,255};
};

using fm_frame = std::array<fm_note, 9>;

class fm_stream {
private:
  struct implementation;
  std::unique_ptr<implementation> state;

public:
  explicit fm_stream(unsigned int sample_rate, fm_backend backend = fm_backend::nuked);
  ~fm_stream();
  void configure_music(std::span<std::byte const> driver, std::array<std::vector<std::byte>,6> songs);
  void configure_gus_music(std::filesystem::path const &patch_directory, std::array<std::vector<std::byte>,6> songs, unsigned int ram_kib = 1024);
  void configure_awe32_music(std::filesystem::path const &rom, std::span<std::byte const> driver, std::array<std::vector<std::byte>,6> songs);
  void configure_sc55_music(std::filesystem::path const &rom_directory, std::array<std::vector<std::byte>,6> songs, sound_canvas_model model = sound_canvas_model::sc55);
  void configure_sampled_music(music_variant variant, std::filesystem::path const &soundfont, std::array<std::vector<std::byte>,6> songs);
  void configure_roland_music(std::filesystem::path const &rom_directory, std::span<std::byte const> driver, std::array<std::vector<std::byte>,6> songs, std::filesystem::path const &percussion_font = {}, std::filesystem::path const &percussion_bank = {}, std::filesystem::path const &setup_bank = {});
  void select_music(int group) noexcept;
  bool publish(fm_frame const &frame) noexcept;
  void render(std::span<float> stereo) noexcept;
};

} // namespace darker::audio
