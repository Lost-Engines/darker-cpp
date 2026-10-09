# Compile the unmodified single-header synthesiser separately from first-party warning settings.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/tinysoundfont.c" "#define TSF_IMPLEMENTATION\n#include <tsf.h>\n")
add_library(tinysoundfont STATIC "${CMAKE_CURRENT_BINARY_DIR}/tinysoundfont.c")
target_include_directories(tinysoundfont SYSTEM PUBLIC "${tinysoundfont_SOURCE_DIR}")
if(UNIX)
  target_link_libraries(tinysoundfont PRIVATE m)
endif()
