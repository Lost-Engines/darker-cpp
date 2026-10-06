#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <boost/program_options.hpp>
#include "graphics/camera.h"
#include "graphics/model_renderer.h"
#include "resources/archive_set.h"
#include "resources/geometry_bank.h"
#include "reference/camera_samples.h"
#include "reference/geometry_bank_samples.h"
#include "reference/original_model_samples.h"

auto main(int const argc, char const *const argv[])->int try {
  /// Decode every original resource and optionally compare independently verified reference bytes
  boost::program_options::options_description options{"Original resource verification"};
  options.add_options()
    ("help,h", "show usage")
    ("data-dir", boost::program_options::value<std::string>()->default_value("."), "directory containing DARKER.00 through DARKER.04 (default: current working directory)")
    ("reference", boost::program_options::value<std::string>(), "directory of independently decoded NN_NNN.bin files");
  boost::program_options::variables_map arguments;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, options), arguments);
  if(arguments.contains("help")) {
    std::cout << options << std::endl;
    return EXIT_SUCCESS;
  }
  boost::program_options::notify(arguments);
  darker::resources::archive_set const archives{arguments["data-dir"].as<std::string>()};
  std::size_t total{0};
  for(auto const &entry : darker::resources::resource_directory()) {
    auto const decoded{archives.load(entry.id)};
    total += decoded.size();
    if(arguments.contains("reference")) {
      auto const path{std::filesystem::path{arguments["reference"].as<std::string>()} / std::format("{:02}_{:03}.bin", entry.id.archive, entry.id.slot)};
      auto const expected{darker::resources::read_binary_file(path, 8'000'000)};
      if(decoded != expected) {
        auto const mismatch{std::ranges::mismatch(decoded, expected)};
        std::cerr << std::format("ERROR: resource {} / {} differs at byte {} (decoded {}, reference {})", entry.id.archive, entry.id.slot, mismatch.in1 - decoded.begin(), decoded.size(), expected.size()) << std::endl;
        return EXIT_FAILURE;
      }
    }
  }
  for(auto const &sample : darker::test_reference::geometry_bank_samples) {
    darker::resources::geometry_bank const bank{archives.load({.archive{0}, .slot{sample.slot}})};
    if(bank.city_types().size() != sample.types || bank.special_models().size() != sample.specials
      || bank.model_pool().size() != sample.pool_size || bank.world_data().size() != sample.tail_size) {
      throw std::runtime_error{"Geometry bank directory differs from the original reference"};
    }
    std::uint64_t fingerprint{0xcbf29ce484222325};
    for(unsigned int type{1}; type <= sample.types; ++type) {
      for(unsigned int state{0}; state < 256; ++state) {
        auto const offset{bank.city_model_offset(type, static_cast<std::uint8_t>(state), static_cast<std::uint8_t>(sample.mask))};
        for(auto const byte : {offset & 255, offset >> 8}) fingerprint = (fingerprint ^ byte) * 0x100000001b3;
      }
    }
    if(fingerprint != sample.fingerprint) throw std::runtime_error{"City model state selection differs from the native reference"};
  }
  for(auto const &sample : darker::test_reference::original_model_samples) {
    darker::resources::geometry_bank const bank{archives.load({.archive{0}, .slot{sample.slot}})};
    darker::graphics::projection_parameters const projection{
      .axes{{
        {.horizontal{static_cast<std::int16_t>(sample.view ? -16384 : 16384)}, .vertical{static_cast<std::int16_t>(sample.view ? -4096 : 4096)}},
        {.horizontal{4096}, .vertical{static_cast<std::int16_t>(sample.view ? 4096 : -4096)}},
        {.vertical{16384}},
      }},
      .horizontal{.fraction{11}}, .vertical{.fraction{19}}, .depth{.whole{1024}}, .origin{.x{160}, .y{120}},
    };
    darker::graphics::model_colours colours{.dynamic{17}};
    for(std::size_t i{0}; i < colours.shades.size(); ++i) colours.shades[i] = static_cast<std::uint8_t>(i);
    framework::render::indexed_cockpit_framebuffer frame{};
    try {
      darker::graphics::draw_flat_model(frame, bank.model_pool(), bank.city_model_offset(sample.type, 0, 0x20), projection, colours);
    } catch(std::exception const &error) {
      throw std::runtime_error{std::format("Bank {}, type {}, view {}: {}", sample.slot, sample.type, sample.view, error.what())};
    }
    std::uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const pixel : frame.pixels) fingerprint = (fingerprint ^ pixel) * 0x100000001b3;
    if(fingerprint != sample.fingerprint) {
      throw std::runtime_error{std::format("Original model drawing differs from native reference: bank {}, type {}, view {} (got {:016x}, expected {:016x})",
        sample.slot, sample.type, sample.view, fingerprint, sample.fingerprint)};
    }
  }
  for(auto const &sample : darker::test_reference::camera_model_samples) {
    darker::resources::geometry_bank const bank{archives.load({.archive{0}, .slot{sample.slot}})};
    darker::graphics::projection_parameters const projection{
      .axes{darker::graphics::make_camera_basis({.heading{static_cast<std::uint16_t>(sample.heading)}, .pitch{static_cast<std::uint16_t>(sample.pitch)}})},
      .depth{.whole{2048}}, .origin{.x{160}, .y{110}},
    };
    darker::graphics::model_colours colours{.dynamic{17}};
    for(std::size_t i{0}; i < colours.shades.size(); ++i) colours.shades[i] = static_cast<std::uint8_t>(i);
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_flat_model(frame, bank.model_pool(), bank.city_model_offset(30, 0, 0x20), projection, colours, sample.slot == 30 ? 168 : 180);
    std::uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const pixel : frame.pixels) fingerprint = (fingerprint ^ pixel) * 0x100000001b3;
    if(fingerprint != sample.fingerprint) {
      throw std::runtime_error{std::format("Camera/model drawing differs from native reference: bank {}, heading {}, pitch {}", sample.slot, sample.heading, sample.pitch)};
    }
  }
  std::cout << std::format("{} camera/model frames match native drawing.", darker::test_reference::camera_model_samples.size()) << std::endl;
  std::cout << std::format("{} original model frames match native drawing.", darker::test_reference::original_model_samples.size()) << std::endl;
  std::cout << "All three geometry banks match native model selection for every city type/state." << std::endl;
  std::cout << std::format("Decoded {} resources: {} bytes", darker::resources::resource_directory().size(), total) << std::endl;
  if(arguments.contains("reference")) std::cout << "All resources match reference files byte-for-byte." << std::endl;
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
