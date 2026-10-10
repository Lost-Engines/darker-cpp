# Use the standalone EMU8000 device, without the DOSBox frontend or machine.
file(READ "${dosbox_emu8k_SOURCE_DIR}/src/hardware/emu8k.cpp" emu8k_source)
# Darker's 1994 library initialises only revision 0x0c; 0x1c silently skips setup.
string(REPLACE "0x1c | ((emu8k->id" "0x0c | ((emu8k->id" emu8k_source "${emu8k_source}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/emu8k.cpp" "${emu8k_source}")
configure_file("${dosbox_emu8k_SOURCE_DIR}/include/emu8k.h" "${CMAKE_CURRENT_BINARY_DIR}/emu8k/emu8k.h" COPYONLY)
add_library(emu8k_backend STATIC "${CMAKE_CURRENT_BINARY_DIR}/emu8k.cpp")
target_include_directories(emu8k_backend SYSTEM PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/emu8k"
  "${CMAKE_CURRENT_BINARY_DIR}/emu8k"
)
target_compile_options(emu8k_backend PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O2;-Wno-write-strings;-Wno-register>)
