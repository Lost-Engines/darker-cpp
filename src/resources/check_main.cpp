#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include "archive_set.h"

auto main(int const argc, char const *const argv[])->int try {
  /// Decode every original resource and optionally compare independently verified reference bytes
  boost::program_options::options_description options{"Original resource verification"};
  options.add_options()
    ("help,h", "show usage")
    ("data-dir", boost::program_options::value<std::string>()->required(), "directory containing DARKER.00 through DARKER.04")
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
  std::cout << std::format("Decoded {} resources: {} bytes", darker::resources::resource_directory().size(), total) << std::endl;
  if(arguments.contains("reference")) std::cout << "All resources match reference files byte-for-byte." << std::endl;
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
