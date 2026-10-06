cmake_minimum_required(VERSION 3.28)
project(darker VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
find_package(Boost 1.85 CONFIG REQUIRED COMPONENTS program_options)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)
include(cmake/dependencies.cmake)

function(configure_first_party target)
  target_compile_features(${target} PUBLIC cxx_std_23)
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wconversion>
  )
endfunction()

# Third-party device implementation, compiled separately from first-party warning settings
add_library(pcm_device STATIC src/platform/miniaudio_implementation.c)
target_include_directories(pcm_device SYSTEM PUBLIC "${miniaudio_SOURCE_DIR}")
target_compile_definitions(pcm_device PUBLIC
  MA_NO_ENGINE
  MA_NO_RESOURCE_MANAGER
  MA_NO_NODE_GRAPH
  MA_NO_DECODING
  MA_NO_ENCODING
  MA_NO_GENERATION
  MA_NO_NULL
)
target_link_libraries(pcm_device
  PRIVATE Threads::Threads
  PRIVATE ${CMAKE_DL_LIBS}
)
if(UNIX)
  target_link_libraries(pcm_device PRIVATE m)
endif()

# Platform-independent framebuffer presentation data
add_library(framework_core STATIC
  src/render/framebuffer.cpp
  src/render/framebuffer.h
  src/render/indexed_framebuffer.h
)
target_include_directories(framework_core PUBLIC src)
configure_first_party(framework_core)

# Window, input, presentation and PCM device adapters
add_library(framework_platform STATIC
  src/platform/audio_output.cpp
  src/platform/audio_output.h
  src/platform/framebuffer_presenter.cpp
  src/platform/framebuffer_presenter.h
)
target_link_libraries(framework_platform
  PUBLIC framework_core
  PRIVATE glfw
  PRIVATE OpenGL::GL
  PRIVATE pcm_device
)
configure_first_party(framework_platform)

# Original resource access has no dependency on windowing or audio
add_library(darker_resources STATIC
  src/resources/geometry_bank.cpp
  src/resources/geometry_bank.h
  src/resources/archive_set.cpp
  src/resources/archive_set.h
  src/resources/decoder.cpp
  src/resources/decoder.h
  src/resources/directory.h
)
target_include_directories(darker_resources PUBLIC src)
configure_first_party(darker_resources)

add_library(darker_graphics STATIC
  src/graphics/bitmap_hud.cpp
  src/graphics/bitmap_hud.h
  src/graphics/blit.cpp
  src/graphics/blit.h
  src/graphics/model_projection.cpp
  src/graphics/model_projection.h
  src/graphics/flat_polygon.cpp
  src/graphics/flat_polygon.h
  src/graphics/cockpit.cpp
  src/graphics/cockpit.h
  src/graphics/cockpit_tables.h
  src/graphics/navigation_hud.cpp
  src/graphics/navigation_hud.h
  src/graphics/procedural_hud.cpp
  src/graphics/procedural_hud.h
  src/graphics/palette_bitmap.cpp
  src/graphics/palette_bitmap.h
)
target_link_libraries(darker_graphics PUBLIC framework_core)
configure_first_party(darker_graphics)

add_library(darker_game STATIC
  src/game/angular_motion.cpp
  src/game/angular_motion.h
  src/game/flight_motion.cpp
  src/game/flight_motion.h
  src/game/beacon_light.cpp
  src/game/beacon_light.h
  src/game/caero_energy.cpp
  src/game/caero_energy.h
  src/game/object_definition.cpp
  src/game/object_definition.h
  src/game/object_definitions.h
  src/game/object_impact.cpp
  src/game/object_impact.h
  src/game/object_pose.cpp
  src/game/object_pose.h
  src/game/object_list.h
  src/game/player_damage.cpp
  src/game/player_damage.h
  src/game/projectile_expiry.cpp
  src/game/projectile_expiry.h
  src/game/projectile_update.cpp
  src/game/projectile_update.h
  src/game/projectile_steering.cpp
  src/game/projectile_steering.h
  src/game/projectile_motion.cpp
  src/game/projectile_motion.h
  src/game/projectile_pool.cpp
  src/game/projectile_pool.h
  src/game/projectile_placement.cpp
  src/game/projectile_placement.h
  src/maths/direction.cpp
  src/maths/direction.h
  src/maths/direction_table.h
  src/maths/sine_table.h
  src/game/random.cpp
  src/game/random.h
  src/game/skimma_weapons.cpp
  src/game/skimma_weapons.h
)
target_include_directories(darker_game PUBLIC src)
configure_first_party(darker_game)

add_executable(darker src/main.cpp)
target_link_libraries(darker
  PRIVATE darker_game
  PRIVATE darker_graphics
  PRIVATE darker_resources
  PRIVATE framework_platform
  PRIVATE glfw
  PRIVATE Boost::program_options
  PRIVATE Boost::headers
)
configure_first_party(darker)

set(DARKER_INSTALL_DIR "" CACHE PATH "Original DOS installation for optional integration verification")
set(DARKER_REFERENCE_DIR "" CACHE PATH "Independently extracted resources for optional integration verification")

if(BUILD_TESTING)
  add_executable(resource_check tests/resource_check_main.cpp)
  target_link_libraries(resource_check PRIVATE darker_resources PRIVATE Boost::program_options)
  configure_first_party(resource_check)

  add_executable(framework_tests
    tests/support/sine_wave.cpp
    tests/support/sine_wave.h
    tests/sine_wave_test.cpp
    tests/framebuffer_test.cpp
    tests/procedural_hud_test.cpp
    tests/navigation_hud_test.cpp
    tests/skimma_weapons_test.cpp
    tests/object_definition_test.cpp
    tests/object_list_test.cpp
    tests/random_test.cpp
    tests/direction_test.cpp
    tests/geometry_bank_test.cpp
    tests/model_projection_test.cpp
    tests/flat_polygon_test.cpp
    tests/angular_motion_test.cpp
    tests/flight_motion_test.cpp
    tests/beacon_energy_test.cpp
    tests/object_impact_test.cpp
    tests/player_damage_test.cpp
    tests/projectile_expiry_test.cpp
    tests/projectile_update_test.cpp
    tests/projectile_steering_test.cpp
    tests/projectile_motion_test.cpp
    tests/projectile_pool_test.cpp
    tests/projectile_placement_test.cpp
    tests/bitmap_hud_test.cpp
    tests/cockpit_test.cpp
    tests/palette_bitmap_test.cpp
    tests/resource_decoder_test.cpp
  )
  target_link_libraries(framework_tests
    PRIVATE framework_core
    PRIVATE darker_resources
    PRIVATE darker_graphics
    PRIVATE darker_game
    PRIVATE Catch2::Catch2WithMain
  )
  configure_first_party(framework_tests)
  include("${catch2_SOURCE_DIR}/extras/Catch.cmake")
  catch_discover_tests(framework_tests)
  if(DARKER_INSTALL_DIR AND DARKER_REFERENCE_DIR)
    add_test(NAME resources.original_pack_comparison
      COMMAND resource_check --data-dir "${DARKER_INSTALL_DIR}" --reference "${DARKER_REFERENCE_DIR}"
    )
    set_tests_properties(resources.original_pack_comparison PROPERTIES LABELS integration)
  endif()
endif()
