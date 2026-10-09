include(GNUInstallDirs)

# Deliberately install only the game, documentation and dependency notices.
# Original packs, saves, analysis captures and test executables are never installed.
install(TARGETS darker RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime)
install(FILES README.md style-guide.md DESTINATION "${CMAKE_INSTALL_DOCDIR}" COMPONENT Runtime)
install(DIRECTORY docs/ DESTINATION "${CMAKE_INSTALL_DOCDIR}/docs"
  COMPONENT Runtime FILES_MATCHING PATTERN "*.md")
install(FILES docs/runtime-patch-audit.json DESTINATION "${CMAKE_INSTALL_DOCDIR}/docs" COMPONENT Runtime)
install(FILES docs/evidence/hangar_launch_comparison.csv docs/evidence/hangar_launch_comparison.svg
  DESTINATION "${CMAKE_INSTALL_DOCDIR}/docs/evidence" COMPONENT Runtime)
set(notice_directory "${CMAKE_INSTALL_DOCDIR}/third_party")
install(FILES "${glfw_SOURCE_DIR}/LICENSE.md" DESTINATION "${notice_directory}" RENAME glfw.txt COMPONENT Runtime)
install(FILES "${miniaudio_SOURCE_DIR}/LICENSE" DESTINATION "${notice_directory}" RENAME miniaudio.txt COMPONENT Runtime)
install(FILES "${nuked_opl3_SOURCE_DIR}/LICENSE" DESTINATION "${notice_directory}" RENAME nuked-opl3.txt COMPONENT Runtime)
install(FILES "${dosbox_opl_SOURCE_DIR}/COPYING" DESTINATION "${notice_directory}" RENAME dosbox.txt COMPONENT Runtime)
install(FILES third_party/boost-license.txt DESTINATION "${notice_directory}" COMPONENT Runtime)
install(FILES cmake/dependencies.cmake cmake/dbopl.cmake cmake/tinysoundfont.cmake
  DESTINATION "${notice_directory}/source-references" COMPONENT Runtime)
install(FILES src/platform/dbopl/dosbox.h
  DESTINATION "${notice_directory}/source-references" COMPONENT Runtime)

install(FILES "${tinysoundfont_SOURCE_DIR}/LICENSE" DESTINATION "${notice_directory}" RENAME tinysoundfont.txt COMPONENT Runtime)

install(FILES "${munt_SOURCE_DIR}/mt32emu/COPYING.txt" DESTINATION "${notice_directory}" RENAME munt-gpl.txt COMPONENT Runtime)
install(FILES "${munt_SOURCE_DIR}/mt32emu/COPYING.LESSER.txt" DESTINATION "${notice_directory}" RENAME munt-lgpl.txt COMPONENT Runtime)

install(PROGRAMS scripts/fetch-assets.sh DESTINATION "${CMAKE_INSTALL_BINDIR}" RENAME darker-fetch-assets COMPONENT Runtime)

install(FILES
  scripts/darker-retail-packs.sha256
  scripts/game_urls.txt
  scripts/roland_rom_urls.txt
  scripts/awe32_rom_urls.txt
  scripts/awe32.sha256
  scripts/sc55_rom_urls.txt
  scripts/sc55-v121.sha256
  scripts/manual_urls.txt
  DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime
)

install(FILES "${nuked_sc55_SOURCE_DIR}/LICENSE" DESTINATION "${notice_directory}" RENAME nuked-sc55.txt COMPONENT Runtime)

install(FILES "${dosbox_emu8k_SOURCE_DIR}/COPYING" DESTINATION "${notice_directory}" RENAME dosbox-x.txt COMPONENT Runtime)
install(FILES "${unicorn_SOURCE_DIR}/COPYING" DESTINATION "${notice_directory}" RENAME unicorn.txt COMPONENT Runtime)
install(FILES "${unicorn_SOURCE_DIR}/COPYING.LGPL2" DESTINATION "${notice_directory}" RENAME unicorn-lgpl.txt COMPONENT Runtime)
install(FILES cmake/emu8k.cmake DESTINATION "${notice_directory}/source-references" COMPONENT Runtime)
