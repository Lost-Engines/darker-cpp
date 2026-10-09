# Isolate the pinned DOSBox GF1 device from its DOS machine and frontend.
file(READ "${dosbox_opl_SOURCE_DIR}/src/hardware/gus.cpp" gf1_source)
string(FIND "${gf1_source}" "class GUS:public Module_base" frontend_start)
string(SUBSTRING "${gf1_source}" 0 ${frontend_start} gf1_source)
string(REGEX REPLACE "#include[^\n]*\n" "" gf1_source "${gf1_source}")
foreach(global IN ITEMS
  "Bit8u adlib_commandreg;"
  "static MixerChannel * gus_chan;"
  "static Bit8u GUSRam[1024*1024];"
  "static Bit32s AutoAmp = 512;"
  "static Bit16u vol16bit[4096];"
  "static Bit32u pantable[16];"
  "static GUSChannels *guschan[32];"
  "static GUSChannels *curchan;"
)
  string(REPLACE "${global}" "" gf1_source "${gf1_source}")
endforeach()
string(REPLACE "} myGUS;" "};\n#include \"audio/gf1/state.h\"" gf1_source "${gf1_source}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gf1.cpp"
  "#include <stdexcept>\n#include <utility>\n#include \"audio/gf1/compatibility.h\"\nnamespace darker::audio::gf1_detail {\n${gf1_source}\n#include \"audio/gf1/implementation.h\"\n")
add_library(gf1_backend STATIC "${CMAKE_CURRENT_BINARY_DIR}/gf1.cpp")
target_include_directories(gf1_backend PRIVATE src)
target_compile_features(gf1_backend PRIVATE cxx_std_23)
target_compile_options(gf1_backend PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-O2>)
