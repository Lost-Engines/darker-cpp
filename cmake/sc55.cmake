# Build only the emulator backend: no SDL, MIDI device or standalone frontend.
set(NUKED_ENABLE_DECODER2 ON)
set(NUKED_SOURCE "Darker pinned backend")
configure_file("${nuked_sc55_SOURCE_DIR}/src/backend/config.h.in" "${CMAKE_CURRENT_BINARY_DIR}/sc55/config.h")
add_library(sc55_backend STATIC
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/address_modes.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/cache.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/disassemble.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/disassemble_handlers.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/disassemblers_address.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/disassemblers_imm16.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/disassemblers_imm8.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/disassemblers_rn.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/disassemblers_top.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/dispatch.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/dispatchers_aa.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/dispatchers_arn.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/dispatchers_d8d16_rn.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/dispatchers_imm16.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/dispatchers_postinc_rn.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/dispatchers_predec_rn.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/dispatchers_rn.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/dispatchers_top.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/decoder2/instruction_handlers.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/config.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/diagnostics.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/emu.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/file_hashing.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/file_io.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/lcd.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/mcu_interrupt.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/mcu_opcodes.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/mcu_timer.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/mcu.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/pcm.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/rom_io.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/rom.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/standard_romsets.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/submcu.cpp"
  "${nuked_sc55_SOURCE_DIR}/src/backend/sha/sha224-256.c"
)
target_include_directories(sc55_backend SYSTEM PUBLIC
  "${nuked_sc55_SOURCE_DIR}/src/backend"
  "${CMAKE_CURRENT_BINARY_DIR}/sc55"
)
target_compile_features(sc55_backend PRIVATE cxx_std_23)
# Real-time processor emulation must remain optimised in Debug builds too.
target_compile_options(sc55_backend PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-O2>)
