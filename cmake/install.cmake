# The portable distribution contains only the executable and unmodified helpers.
install(TARGETS darker RUNTIME DESTINATION . COMPONENT Runtime)
install(DIRECTORY scripts DESTINATION . USE_SOURCE_PERMISSIONS COMPONENT Runtime)
