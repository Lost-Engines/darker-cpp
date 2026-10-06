cmake_minimum_required(VERSION 3.28)
project(armchair_framework VERSION 0.1.0 LANGUAGES C CXX)

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

# Platform-independent demo signal and framebuffer generation
add_library(framework_core STATIC
  src/audio/sine_wave.cpp
  src/audio/sine_wave.h
  src/render/framebuffer.cpp
  src/render/framebuffer.h
  src/render/indexed_framebuffer.cpp
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
  src/platform/input.cpp
  src/platform/input.h
)
target_link_libraries(framework_platform
  PUBLIC framework_core
  PRIVATE glfw
  PRIVATE OpenGL::GL
  PRIVATE pcm_device
)
configure_first_party(framework_platform)

add_executable(framework_demo src/main.cpp)
target_link_libraries(framework_demo
  PRIVATE framework_platform
  PRIVATE glfw
  PRIVATE Boost::program_options
  PRIVATE Boost::headers
)
configure_first_party(framework_demo)

# Original resource access has no dependency on windowing or audio
add_library(darker_resources STATIC
  src/resources/archive_set.cpp
  src/resources/archive_set.h
  src/resources/decoder.cpp
  src/resources/decoder.h
  src/resources/directory.h
)
target_include_directories(darker_resources PUBLIC src)
configure_first_party(darker_resources)

add_executable(resource_check src/resources/check_main.cpp)
target_link_libraries(resource_check
  PRIVATE darker_resources
  PRIVATE Boost::program_options
)
configure_first_party(resource_check)

add_library(darker_graphics STATIC
  src/graphics/palette_bitmap.cpp
  src/graphics/palette_bitmap.h
)
target_link_libraries(darker_graphics PUBLIC framework_core)
configure_first_party(darker_graphics)

add_executable(bitmap_viewer src/graphics/viewer_main.cpp)
target_link_libraries(bitmap_viewer
  PRIVATE darker_graphics
  PRIVATE darker_resources
  PRIVATE framework_platform
  PRIVATE glfw
  PRIVATE Boost::program_options
  PRIVATE Boost::headers
)
configure_first_party(bitmap_viewer)

set(DARKER_INSTALL_DIR "" CACHE PATH "Original DOS installation for optional integration verification")
set(DARKER_REFERENCE_DIR "" CACHE PATH "Independently extracted resources for optional integration verification")

if(BUILD_TESTING)
  add_executable(framework_tests
    tests/sine_wave_test.cpp
    tests/framebuffer_test.cpp
    tests/palette_bitmap_test.cpp
    tests/resource_decoder_test.cpp
  )
  target_link_libraries(framework_tests
    PRIVATE framework_core
    PRIVATE darker_resources
    PRIVATE darker_graphics
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
