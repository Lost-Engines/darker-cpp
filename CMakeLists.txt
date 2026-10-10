cmake_minimum_required(VERSION 3.28)
project(darker VERSION 0.1.0 LANGUAGES C CXX)
if(APPLE)
  enable_language(OBJC)
endif()

option(DARKER_STATIC_RUNTIME "Link compiler runtimes statically" ${MINGW})
if(DARKER_STATIC_RUNTIME)
  if(MINGW)
    add_link_options(-static)
  else()
    add_link_options("$<$<LINK_LANG_AND_ID:CXX,GNU>:-static-libgcc;-static-libstdc++>")
  endif()
endif()

include(CTest)
find_package(Boost 1.85 CONFIG REQUIRED COMPONENTS program_options)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)
include(cmake/dependencies.cmake)

function(configure_first_party target)
  target_compile_features(${target} PUBLIC cxx_std_23)
  target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}")
  target_link_libraries(${target} PUBLIC Boost::headers)
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Wconversion>
  )
endfunction()

# Third-party device implementation, compiled separately from first-party warning settings
if(APPLE)
  # CoreAudio headers require Apple's blocks extension, as Cocoa does for GLFW.
  set_source_files_properties(src/platform/miniaudio_implementation.c PROPERTIES LANGUAGE OBJC)
endif()
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

# Original OPL register driver and low-level chip emulation
add_library(nuked_opl3 STATIC "${nuked_opl3_SOURCE_DIR}/opl3.c")
target_include_directories(nuked_opl3 SYSTEM PUBLIC "${nuked_opl3_SOURCE_DIR}")
include(cmake/dbopl.cmake)
include(cmake/tinysoundfont.cmake)
include(cmake/sc55.cmake)
include(cmake/emu8k.cmake)
include(cmake/gf1.cmake)
add_library(darker_audio STATIC
  src/audio/ambient_sounds.cpp
  src/audio/ambient_sounds.h
  src/audio/gus_synth.cpp
  src/audio/gus_synth.h
  src/audio/awe32_synth.cpp
  src/audio/awe32_synth.h
  src/audio/sc55_synth.cpp
  src/audio/sc55_synth.h
  src/audio/midi_synth.h
  src/audio/roland_patches.cpp
  src/audio/roland_patches.h
  src/audio/roland_synth.cpp
  src/audio/roland_synth.h
  src/audio/midi_music.cpp
  src/audio/midi_music.h
  src/audio/soundfont.cpp
  src/audio/soundfont.h
  src/audio/sound_images.cpp
  src/audio/sound_images.h
  src/audio/voice_allocation.cpp
  src/audio/voice_allocation.h
  src/audio/world_sounds.cpp
  src/audio/world_sounds.h
  src/audio/flight_sounds.cpp
  src/audio/flight_sounds.h
  src/audio/fm_stream.cpp
  src/audio/fm_stream.h
  src/audio/fm_driver.cpp
  src/audio/fm_driver.h
  src/audio/fm_patches.h
  src/audio/fm_synth.cpp
  src/audio/fm_synth.h
)
target_include_directories(darker_audio PUBLIC src)
target_link_libraries(darker_audio
  PUBLIC darker_game
  PRIVATE nuked_opl3
  PRIVATE dosbox_opl_core
  PRIVATE gf1_backend
  PRIVATE emu8k_backend
  PRIVATE unicorn
  PRIVATE sc55_backend
  PRIVATE tinysoundfont
  PRIVATE MT32Emu::mt32emu
  PRIVATE Boost::headers
)
configure_first_party(darker_audio)

# Platform-independent framebuffer presentation data
add_library(framework_core STATIC
  src/render/framebuffer.cpp
  src/render/framebuffer.h
  src/render/indexed_framebuffer.h
  src/render/indexed_surface.h
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
  src/resources/save_file.cpp
  src/resources/save_file.h
  src/resources/font.cpp
  src/resources/font.h
  src/resources/campaign.cpp
  src/resources/campaign.h
  src/resources/scenario.cpp
  src/resources/scenario.h
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

add_library(darker_maths STATIC
  src/maths/view_basis.cpp
  src/maths/view_basis.h
)
target_include_directories(darker_maths PUBLIC src)
configure_first_party(darker_maths)

add_library(darker_graphics STATIC
  src/graphics/formatted_text.cpp
  src/graphics/formatted_text.h
  src/graphics/font.cpp
  src/graphics/font.h
  src/graphics/bitmap_hud.cpp
  src/graphics/bitmap_hud.h
  src/graphics/blit.cpp
  src/graphics/blit.h
  src/graphics/sky_ground.cpp
  src/graphics/sky_ground.h
  src/graphics/screen_primitives.cpp
  src/graphics/screen_primitives.h
  src/graphics/near_clip.cpp
  src/graphics/near_clip.h
  src/graphics/model_lighting.cpp
  src/graphics/model_lighting.h
  src/graphics/model_renderer.cpp
  src/graphics/model_renderer.h
  src/graphics/particles.cpp
  src/graphics/particles.h
  src/graphics/particle_tables.h
  src/graphics/city_scene.cpp
  src/graphics/city_scene.h
  src/graphics/city_visibility.cpp
  src/graphics/city_visibility.h
  src/graphics/raster_arithmetic.h
  src/graphics/raster_viewport.h
  src/graphics/render_geometry.h
  src/graphics/screen_layout.h
  src/graphics/tunnel_visibility.cpp
  src/graphics/tunnel_visibility.h
  src/graphics/camera.cpp
  src/graphics/camera.h
  src/graphics/model_projection.cpp
  src/graphics/model_projection.h
  src/graphics/flat_polygon.cpp
  src/graphics/gouraud_polygon.cpp
  src/graphics/gouraud_polygon.h
  src/graphics/flat_polygon.h
  src/graphics/flight_instruments.cpp
  src/graphics/flight_instruments.h
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
target_link_libraries(darker_graphics PUBLIC framework_core PUBLIC darker_resources PUBLIC darker_maths)
configure_first_party(darker_graphics)

add_library(darker_game STATIC
  src/game/city_persistence.cpp
  src/game/city_persistence.h
  src/game/aircraft_spawning.cpp
  src/game/aircraft_spawning.h
  src/game/actor_activation.cpp
  src/game/actor_activation.h
  src/game/actor_motion.cpp
  src/game/actor_motion.h
  src/game/aircraft_combat.cpp
  src/game/aircraft_combat.h
  src/game/actor_update.cpp
  src/game/actor_update.h
  src/game/vehicle_combat.cpp
  src/game/vehicle_route.cpp
  src/game/vehicle_route.h
  src/game/actor_navigation.cpp
  src/game/actor_navigation.h
  src/game/tunnel_flight.cpp
  src/game/tunnel_flight.h
  src/game/tunnel_network.cpp
  src/game/tunnel_network.h
  src/game/tunnel_navigation.cpp
  src/game/tunnel_navigation.h
  src/game/tunnel_portal.cpp
  src/game/tunnel_portal.h
  src/game/scenario_world.cpp
  src/game/scenario_world.h
  src/game/supply_pad.cpp
  src/game/supply_pad.h
  src/game/scenario_setup.cpp
  src/game/scenario_setup.h
  src/game/scenario_actor.cpp
  src/game/scenario_actor.h
  src/game/actor_awareness.cpp
  src/game/actor_awareness.h
  src/game/effect_sound.cpp
  src/game/effects.cpp
  src/game/effects.h
  src/game/effect_sound.h
  src/game/effect_tables.h
  src/game/mission_combat.cpp
  src/game/mission_combat.h
  src/game/mission_exchange.cpp
  src/game/mission_exchange.h
  src/game/mission_script.cpp
  src/game/mission_script.h
  src/game/hangar.cpp
  src/game/hangar.h
  src/game/city_map.cpp
  src/game/city_map.h
  src/game/angular_motion.cpp
  src/game/angular_motion.h
  src/game/flight_motion.cpp
  src/game/camera_target.cpp
  src/game/camera_target.h
  src/game/flight_camera.cpp
  src/game/flight_camera.h
  src/game/flight_controls.cpp
  src/game/flight_controls.h
  src/game/flight_motion.h
  src/game/radar_coverage.cpp
  src/game/radar_coverage.h
  src/graphics/radar_beacons.cpp
  src/graphics/radar_beacons.h
  src/game/beacon_changes.cpp
  src/game/beacon_changes.h
  src/game/beacon_light.cpp
  src/game/beacon_light.h
  src/game/weapon_target.cpp
  src/game/weapon_target.h
  src/game/building_impact.cpp
  src/game/caero_weapons.cpp
  src/game/caero_weapons.h
  src/game/caero_energy.cpp
  src/game/collision_cells.cpp
  src/game/collision_cells.h
  src/game/collision_box.h
  src/game/collision_sweep.cpp
  src/game/collision_sweep.h
  src/game/city_sweep.cpp
  src/game/city_sweep.h
  src/game/city_collision.cpp
  src/game/city_collision.h
  src/game/game_clock.cpp
  src/game/game_clock.h
  src/game/flight_attitude.cpp
  src/game/flight_attitude.h
  src/game/skimma_flight.cpp
  src/game/skimma_flight.h
  src/game/caero_flight.cpp
  src/game/caero_flight.h
  src/game/caero_energy.h
  src/game/object_deadline.cpp
  src/game/object_deadline.h
  src/game/objective_counter.h
  src/game/object_definition.cpp
  src/game/object_definition.h
  src/game/object_definitions.h
  src/game/object_impact.cpp
  src/game/object_impact.h
  src/game/object_pose.cpp
  src/game/object_pose.h
  src/game/object_list.h
  src/game/player_flight.cpp
  src/game/player_flight.h
  src/game/player_crash.cpp
  src/game/player_crash.h
  src/game/player_damage.cpp
  src/game/player_damage.h
  src/game/projectile_expiry.cpp
  src/game/projectile_expiry.h
  src/game/projectile_update.cpp
  src/game/projectile_update.h
  src/game/dual_launch.cpp
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
target_link_libraries(darker_game PUBLIC darker_maths)
target_include_directories(darker_game PUBLIC src)
configure_first_party(darker_game)

add_library(darker_presentation STATIC
  src/presentation/player.cpp
  src/presentation/player.h
  src/presentation/front_end.cpp
  src/presentation/front_end.h
)
target_include_directories(darker_presentation PUBLIC src)
target_link_libraries(darker_presentation PUBLIC darker_game darker_graphics darker_resources)
configure_first_party(darker_presentation)

add_executable(darker src/main.cpp)
target_link_libraries(darker
  PRIVATE darker_presentation
  PRIVATE darker_audio
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
set(DARKER_TEST_MT32_ROM_DIR "" CACHE PATH "Optional Roland ROM directory for Munt PCM verification")
set(DARKER_TEST_ROLAND_GM_BANK "" CACHE FILEPATH "Optional Roland MTGM.MID bank for percussion fallback verification")
set(DARKER_TEST_GUS_DIR "" CACHE PATH "Optional UltraMID and patch directory for Gravis verification")
set(DARKER_TEST_AWE32_ROM "" CACHE FILEPATH "Optional AWE32 ROM for native synthesis verification")
set(DARKER_TEST_SCC1A_ROM_DIR "" CACHE PATH "Optional SCC-1A v1.30 ROM directory for PCM verification")
set(DARKER_TEST_SC55_ROM_DIR "" CACHE PATH "Optional SC-55 v1.21 ROM directory for PCM verification")
set(DARKER_TEST_SOUNDFONT "" CACHE FILEPATH "Optional GM SoundFont for sampled music PCM verification")

if(BUILD_TESTING)
  add_executable(resource_check
    tests/music_check.cpp
    tests/music_check.h
    tests/city_persistence_check.cpp
    tests/city_persistence_check.h
    tests/skimma_combat_check.cpp
    tests/skimma_combat_check.h
    tests/building_combat_check.cpp
    tests/building_combat_check.h
    tests/mission_combat_check.cpp
    tests/mission_combat_check.h
    tests/actor_flight_check.cpp
    tests/actor_flight_check.h
    tests/resource_check_main.cpp
    tests/scenario_resource_check.cpp
    tests/scenario_resource_check.h
    tests/nightmare_check.cpp
    tests/nightmare_check.h
    tests/camera_target_check.cpp
    tests/camera_target_check.h
    tests/presentation_check.cpp
    tests/presentation_check.h
    tests/text_resource_check.cpp
    tests/text_resource_check.h
    tests/hangar_flight_check.cpp
    tests/hangar_flight_check.h
  )
  target_compile_definitions(resource_check PRIVATE DARKER_TEST_SC55_ROM_DIR="${DARKER_TEST_SC55_ROM_DIR}" DARKER_TEST_ROLAND_GM_BANK="${DARKER_TEST_ROLAND_GM_BANK}" DARKER_TEST_MT32_ROM_DIR="${DARKER_TEST_MT32_ROM_DIR}" DARKER_TEST_SOUNDFONT="${DARKER_TEST_SOUNDFONT}")
  target_link_libraries(resource_check PRIVATE darker_audio PRIVATE darker_presentation PRIVATE darker_game PRIVATE darker_resources PRIVATE darker_graphics PRIVATE Boost::program_options)
  configure_first_party(resource_check)

  add_executable(framework_tests
    tests/gus_synth_test.cpp
    tests/awe32_synth_test.cpp
    tests/sc55_synth_test.cpp
    tests/roland_patches_test.cpp
    tests/save_file_test.cpp
    tests/world_sounds_test.cpp
    tests/ambient_sounds_test.cpp
    tests/effects_test.cpp
    tests/aircraft_combat_test.cpp
    tests/actor_activation_test.cpp
    tests/actor_motion_test.cpp
    tests/vehicle_combat_test.cpp
    tests/vehicle_route_test.cpp
    tests/actor_navigation_test.cpp
    tests/tunnel_network_test.cpp
    tests/supply_pad_test.cpp
    tests/scenario_setup_test.cpp
    tests/scenario_actor_test.cpp
    tests/actor_awareness_test.cpp
    tests/objective_counter_test.cpp
    tests/mission_exchange_test.cpp
    tests/mission_script_test.cpp
    tests/hangar_test.cpp
    tests/font_test.cpp
    tests/scenario_world_test.cpp
    tests/scenario_test.cpp
    tests/support/sine_wave.cpp
    tests/support/sine_wave.h
    tests/sine_wave_test.cpp
    tests/fm_driver_test.cpp
    tests/framebuffer_test.cpp
    tests/procedural_hud_test.cpp
    tests/radar_beacons_test.cpp
    tests/navigation_hud_test.cpp
    tests/skimma_weapons_test.cpp
    tests/object_definition_test.cpp
    tests/object_list_test.cpp
    tests/random_test.cpp
    tests/direction_test.cpp
    tests/geometry_bank_test.cpp
    tests/collision_cells_test.cpp
    tests/collision_sweep_test.cpp
    tests/city_sweep_test.cpp
    tests/city_collision_test.cpp
    tests/city_scene_test.cpp
    tests/tunnel_visibility_test.cpp
    tests/camera_test.cpp
    tests/flight_camera_test.cpp
    tests/sky_ground_test.cpp
    tests/screen_primitives_test.cpp
    tests/near_clip_test.cpp
    tests/model_animation_test.cpp
    tests/model_lighting_test.cpp
    tests/model_renderer_test.cpp
    tests/model_projection_test.cpp
    tests/flat_polygon_test.cpp
    tests/gouraud_polygon_test.cpp
    tests/angular_motion_test.cpp
    tests/flight_motion_test.cpp
    tests/flight_controls_test.cpp
    tests/game_clock_test.cpp
    tests/skimma_flight_test.cpp
    tests/mimic_test.cpp
    tests/weapon_target_test.cpp
    tests/building_impact_test.cpp
    tests/dual_launch_test.cpp
    tests/caero_weapons_test.cpp
    tests/chargeable_test.cpp
    tests/caero_flight_test.cpp
    tests/beacon_changes_test.cpp
    tests/beacon_energy_test.cpp
    tests/object_impact_test.cpp
    tests/player_crash_test.cpp
    tests/player_damage_test.cpp
    tests/projectile_expiry_test.cpp
    tests/projectile_update_test.cpp
    tests/projectile_steering_test.cpp
    tests/projectile_motion_test.cpp
    tests/projectile_pool_test.cpp
    tests/projectile_placement_test.cpp
    tests/bitmap_hud_test.cpp
    tests/flight_instruments_test.cpp
    tests/cockpit_test.cpp
    tests/palette_bitmap_test.cpp
    tests/resource_decoder_test.cpp
  )
  target_link_libraries(framework_tests
    PRIVATE darker_presentation
    PRIVATE darker_audio
    PRIVATE framework_core
    PRIVATE darker_resources
    PRIVATE darker_graphics
    PRIVATE darker_game
    PRIVATE Catch2::Catch2WithMain
  )
  configure_first_party(framework_tests)
  include("${catch2_SOURCE_DIR}/extras/Catch.cmake")
  target_compile_definitions(framework_tests PRIVATE DARKER_TEST_SCC1A_ROM_DIR="${DARKER_TEST_SCC1A_ROM_DIR}" DARKER_TEST_GUS_DIR="${DARKER_TEST_GUS_DIR}" DARKER_TEST_AWE32_ROM="${DARKER_TEST_AWE32_ROM}" DARKER_TEST_RESOURCE_DIR="${DARKER_REFERENCE_DIR}" DARKER_TEST_SC55_ROM_DIR="${DARKER_TEST_SC55_ROM_DIR}")
  catch_discover_tests(framework_tests)
  if(DARKER_INSTALL_DIR AND DARKER_REFERENCE_DIR)
    add_test(NAME resources.original_pack_comparison
      COMMAND resource_check --data-dir "${DARKER_INSTALL_DIR}" --reference "${DARKER_REFERENCE_DIR}"
    )
    set_tests_properties(resources.original_pack_comparison PROPERTIES LABELS integration)
  endif()
endif()

include(cmake/install.cmake)
