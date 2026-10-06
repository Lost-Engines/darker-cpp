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
#include "resources/archive_set.h"
#include "resources/geometry_bank.h"
#include "reference/geometry_bank_samples.h"

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
  std::cout << "All three geometry banks match native model selection for every city type/state." << std::endl;
  std::cout << std::format("Decoded {} resources: {} bytes", darker::resources::resource_directory().size(), total) << std::endl;
  if(arguments.contains("reference")) std::cout << "All resources match reference files byte-for-byte." << std::endl;
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
