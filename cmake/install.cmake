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
install(FILES cmake/dependencies.cmake cmake/dbopl.cmake
  DESTINATION "${notice_directory}/source-references" COMPONENT Runtime)
install(FILES src/platform/dbopl/dosbox.h
  DESTINATION "${notice_directory}/source-references" COMPONENT Runtime)
