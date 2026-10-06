#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <boost/program_options.hpp>
#include "game/city_collision.h"
#include "game/city_sweep.h"
#include "game/player_flight.h"
#include "graphics/camera.h"
#include "graphics/city_scene.h"
#include "graphics/model_renderer.h"
#include "reference/camera_samples.h"
#include "reference/city_collision_samples.h"
#include "reference/city_frame_samples.h"
#include "reference/city_placement_samples.h"
#include "reference/city_sweep_samples.h"
#include "reference/geometry_bank_samples.h"
#include "reference/model_effect_samples.h"
#include "reference/original_model_samples.h"
#include "reference/player_flight_samples.h"
#include "resources/archive_set.h"
#include "resources/geometry_bank.h"

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
  for(unsigned int const slot : {30, 31, 32}) {
    darker::resources::geometry_bank const bank{archives.load({.archive{0}, .slot{slot}})};
    for(auto const &sample : darker::test_reference::city_collision_samples) {
      if(sample.slot != slot) continue;
      auto const boxes{darker::game::city_collision_boxes(bank, sample.type, static_cast<std::uint8_t>(sample.state),
        static_cast<std::uint8_t>(sample.slot == 30 ? 0x20 : 0x60), 20, 20, static_cast<std::uint16_t>(sample.expansion))};
      std::uint64_t fingerprint{0xcbf29ce484222325};
      for(auto const &box : boxes) {
        for(unsigned int const value : {box.minimum[0], box.minimum[1], box.minimum[2], box.maximum[0], box.maximum[1], box.maximum[2], static_cast<std::uint16_t>(box.category)}) {
          for(auto const byte : {value & 255, value >> 8}) fingerprint = (fingerprint ^ byte) * 0x100000001b3;
        }
      }
      if(boxes.size() != sample.count || fingerprint != sample.fingerprint) {
        throw std::runtime_error{std::format("City collision decoding differs from native reference: bank {}, type {}, state {}, expansion {}", sample.slot, sample.type, sample.state, sample.expansion)};
      }
    }
  }
  for(unsigned int const slot : {30, 31, 32}) {
    darker::resources::geometry_bank const bank{archives.load({.archive{0}, .slot{slot}})};
    std::array<darker::game::city_cell, 128 * 128> cells{};
    for(auto const &sample : darker::test_reference::city_sweep_samples) {
      if(sample.slot != slot) continue;
      cells[20 * 128 + 20] = {.type{static_cast<std::uint8_t>(sample.type)}, .state{static_cast<std::uint8_t>(sample.state)}};
      auto const words{[](std::array<int, 3> const &values){
        return std::array<std::uint16_t, 3>{static_cast<std::uint16_t>(values[0]), static_cast<std::uint16_t>(values[1]), static_cast<std::uint16_t>(values[2])};
      }};
      auto end{words(sample.end)};
      auto const result{darker::game::sweep_city(bank, cells, static_cast<std::uint8_t>(slot == 30 ? 0x20 : 0x60), words(sample.start), end)};
      if(static_cast<unsigned int>(result.contact) != sample.hit || result.category != sample.category || end != words(sample.result)) {
        throw std::runtime_error{std::format("City sweep differs from native reference: bank {}, type {}, state {}, start ({},{},{}), got contact {} category {} end ({},{},{}), expected {} {} ({},{},{})",
          slot, sample.type, sample.state, sample.start[0], sample.start[1], sample.start[2], static_cast<unsigned int>(result.contact), result.category, end[0], end[1], end[2],
          sample.hit, sample.category, sample.result[0], sample.result[1], sample.result[2])};
      }
    }
  }
  {
    darker::resources::geometry_bank const bank{archives.load({.archive{0}, .slot{30}})};
    darker::game::city_map cells{};
    darker::game::player_flight player;
    for(auto const &sample : darker::test_reference::player_flight_samples) {
      auto const &input{sample.input};
      if(input[4] == 8) {
        cells.fill({});
        cells[20 * 128 + 20] = {.type{static_cast<std::uint8_t>(input[1])}};
        player = {};
        darker::game::object_pose const pose{.position{5248, 5504, static_cast<std::uint16_t>(input[2])}, .speed{1500}};
        if(input[0] == 25) player.craft = darker::game::caero_flight_state{.pose{pose}, .horizontal_velocity{1500}, .flying{true}};
        else player.craft = darker::game::skimma_flight_state{.pose{pose}, .damage{.shield_charge{0xbf00}}, .horizontal_velocity{1500}};
        player.upgraded = input[0] == 27;
        player.engine_flags = static_cast<std::uint8_t>(input[3]);
        player.forward_setting = 256;
      }
      player.advance({}, false, 8, static_cast<std::uint16_t>(input[4]), bank, cells);
      auto const velocity{std::visit([](auto const &state){ return std::array<int, 2>{state.vertical_velocity, state.horizontal_velocity}; }, player.craft)};
      auto const &pose{player.pose()};
      if(std::array<int, 3>{pose.position[0], pose.position[1], pose.position[2]} != sample.position || velocity != sample.velocity
        || std::array<int, 3>{pose.angles[0], pose.angles[1], pose.angles[2]} != sample.angles
        || player.lifecycle.crashing != (sample.outcome[0] == 0x6ef7) || cells[20 * 128 + 20].state != sample.outcome[1]) {
        throw std::runtime_error{std::format("Player flight/collision differs from native reference: craft {}, type {}, height {}, engine {}, tick {}: position ({},{},{}) expected ({},{},{})",
          input[0], input[1], input[2], input[3], input[4], pose.position[0], pose.position[1], pose.position[2], sample.position[0], sample.position[1], sample.position[2])};
      }
    }
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
      darker::graphics::draw_model(frame, bank.model_pool(), bank.city_model_offset(sample.type, 0, 0x20), projection, colours);
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
    darker::graphics::draw_model(frame, bank.model_pool(), bank.city_model_offset(30, 0, 0x20), projection, colours, sample.slot == 30 ? 168 : 180);
    std::uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const pixel : frame.pixels) fingerprint = (fingerprint ^ pixel) * 0x100000001b3;
    if(fingerprint != sample.fingerprint) {
      throw std::runtime_error{std::format("Camera/model drawing differs from native reference: bank {}, heading {}, pitch {}", sample.slot, sample.heading, sample.pitch)};
    }
  }
  for(auto const &sample : darker::test_reference::city_placement_samples) {
    darker::resources::geometry_bank const bank{archives.load({.archive{0}, .slot{sample.model[0]}})};
    auto const basis{darker::graphics::make_camera_basis({.heading{sample.angles[0]}, .pitch{sample.angles[1]}, .roll{sample.angles[2]}})};
    auto const item{darker::graphics::place_city_cell(bank,
      {.type{static_cast<std::uint8_t>(sample.model[1])}, .state{static_cast<std::uint8_t>(sample.model[2])}}, 64 * 128 + 64,
      static_cast<std::uint8_t>(sample.model[3]), basis,
      {.column{sample.camera[0]}, .row{sample.camera[1]}, .altitude{std::bit_cast<std::int16_t>(sample.camera[2])}})};
    bool matched{item.has_value() == sample.visible};
    if(item && sample.visible) {
      std::array<std::uint16_t, 10> const values{
        static_cast<std::uint16_t>(item->model_offset), item->placement.horizontal.whole, item->placement.horizontal.fraction,
        item->placement.vertical.whole, item->placement.vertical.fraction, item->placement.depth.whole, item->placement.depth.fraction,
        item->placement.sorting_distance, static_cast<std::uint16_t>(item->path == darker::graphics::model_path::near_clipped ? 0x2c62 : item->force_flat ? 0x2ca9 : 0x2cb3),
        static_cast<std::uint16_t>(item->background),
      };
      matched = values == sample.result;
    }
    if(!matched) throw std::runtime_error{std::format("City placement differs from native reference: bank {}, type {}, state {}", sample.model[0], sample.model[1], sample.model[2])};
  }
  for(auto const &sample : darker::test_reference::model_effect_samples) {
    darker::resources::geometry_bank const bank{archives.load({.archive{0}, .slot{sample.slot}})};
    darker::graphics::projection_parameters const projection{
      .axes{darker::graphics::make_camera_basis({.heading{static_cast<std::uint16_t>(sample.heading)}, .pitch{61440}})},
      .horizontal{.fraction{11}}, .vertical{.fraction{19}},
      .depth{.whole{static_cast<std::uint16_t>(sample.near ? 64 : 2048)}, .fraction{83}}, .origin{.x{160}, .y{84}},
    };
    darker::graphics::model_colours colours{.dynamic{17}};
    for(std::size_t i{0}; i < colours.shades.size(); ++i) colours.shades[i] = static_cast<std::uint8_t>(i);
    darker::graphics::model_animation animation{};
    darker::graphics::update_fountain_parameters(animation, static_cast<std::uint16_t>(sample.clock));
    animation.parameters[0] = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(sample.gate));
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_model(frame, bank.model_pool(), bank.city_model_offset(sample.type, 0, 0x20), projection, colours, 168,
      sample.near ? darker::graphics::model_path::near_clipped : darker::graphics::model_path::direct, animation);
    std::uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const pixel : frame.pixels) fingerprint = (fingerprint ^ pixel) * 0x100000001b3;
    if(fingerprint != sample.fingerprint) throw std::runtime_error{std::format("Model effect differs from native reference: bank {}, type {}, heading {}, gate {}, clock {}, near {} (got {:016x}, expected {:016x})",
      sample.slot, sample.type, sample.heading, sample.gate, sample.clock, sample.near, fingerprint, sample.fingerprint)};
  }
  for(auto const &sample : darker::test_reference::city_frame_samples) {
    darker::resources::geometry_bank const bank{archives.load({.archive{0}, .slot{sample.slot}})};
    auto cells{darker::game::make_city_map(archives.load({.archive{0}, .slot{sample.slot == 30 ? 68u : 69u}}), sample.slot == 30)};
    if(sample.slot == 30) {
      for(std::size_t row{0}; row < 128; row += 9) {
        for(std::size_t column{0}; column < 128; column += 9) {
          auto &cell{cells[row * 128 + column]};
          if(cell.type == 1) cell.state = static_cast<std::uint8_t>(sample.light);
        }
      }
    }
    std::array<std::uint8_t, 256> limits{};
    for(std::size_t i{0}; i < bank.city_types().size(); ++i) limits[i + 1] = bank.city_types()[i].variant_limit;
    darker::game::assign_city_variants(cells, limits);
    std::uint64_t map{0xcbf29ce484222325};
    for(auto const cell : cells) {
      for(auto const byte : {cell.type, cell.state}) map = (map ^ byte) * 0x100000001b3;
    }
    if(map != sample.map) throw std::runtime_error{"City initial state differs from native setup"};
    darker::graphics::city_view const view{
      .column{static_cast<std::uint16_t>(sample.column)}, .row{static_cast<std::uint16_t>(sample.row)},
      .altitude{static_cast<std::int16_t>(sample.altitude)},
      .angles{.heading{static_cast<std::uint16_t>(sample.heading)}, .pitch{static_cast<std::uint16_t>(sample.pitch)}},
      .beacon_lighting{sample.slot == 30}, .gouraud{sample.gouraud != 0},
    };
    darker::graphics::distance_shading const lighting;
    darker::graphics::model_animation animation{};
    darker::graphics::update_fountain_parameters(animation, static_cast<std::uint16_t>(sample.clock));
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::city_renderer scene;
    auto const count{scene.draw(frame, bank, cells, view, sample.slot == 30 ? 0x20 : 0x60, lighting, animation)};
    std::uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const pixel : frame.pixels) fingerprint = (fingerprint ^ pixel) * 0x100000001b3;
    if(count != sample.count || fingerprint != sample.frame) throw std::runtime_error{std::format(
      "City frame differs from native reference: bank {}, column {}, row {} (count {}/{}, frame {:016x}/{:016x})",
      sample.slot, sample.column, sample.row, count, sample.count, fingerprint, sample.frame)};
  }
  std::cout << std::format("{} complete city frames and initial maps match native execution.", darker::test_reference::city_frame_samples.size()) << std::endl;
  std::cout << std::format("{} complete model effect frames match native drawing.", darker::test_reference::model_effect_samples.size()) << std::endl;
  std::cout << std::format("{} city placements and culling decisions match native execution.", darker::test_reference::city_placement_samples.size()) << std::endl;
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
