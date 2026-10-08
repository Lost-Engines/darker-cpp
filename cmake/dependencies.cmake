include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW documentation")
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW examples")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW tests")
set(GLFW_INSTALL OFF CACHE BOOL "Install GLFW")

# GLFW 3.4, miniaudio 0.11.23 and Catch2 3.8.1; immutable archives with verified hashes
FetchContent_Declare(glfw
  URL https://codeload.github.com/glfw/glfw/tar.gz/a74efa0d5628b74adc0426af4c5710e287fa7c2c
  URL_HASH SHA256=7e85e39ade1efdda00734d323a0230b5c938074e9fb2ddbe453bdbb281064d2c
)
FetchContent_Declare(miniaudio
  URL https://codeload.github.com/mackron/miniaudio/tar.gz/f40cf03f80cdb7e741d43e53b7e706e8c1394bcf
  URL_HASH SHA256=412326cf55133404cbfb81ec8974b10149dc68732ce5bdeee8ba9cfc2695d646
  SOURCE_SUBDIR unused_upstream_build
)
FetchContent_Declare(nuked_opl3
  URL https://codeload.github.com/nukeykt/Nuked-OPL3/tar.gz/765ec962e473aeb767e4cba74ffdc8f588ffbfe8
  URL_HASH SHA256=2fad908c3904d3ef51b55e0d6a8980c7142942e2d0baa7e99bf038cf0a41199d
  SOURCE_SUBDIR unused_upstream_build
)
FetchContent_Declare(dosbox_opl
  URL https://codeload.github.com/dosbox-staging/dosbox-staging/tar.gz/e164e788f9819d5ab898d705f863e2046baf8b03
  URL_HASH SHA256=efed2dc8c2807503352e817f3160dcad6236824ba4f16347a260bb9293965c50
  SOURCE_SUBDIR unused_upstream_build
)
FetchContent_MakeAvailable(glfw miniaudio nuked_opl3 dosbox_opl)

if(BUILD_TESTING)
  FetchContent_Declare(catch2
    URL https://codeload.github.com/catchorg/Catch2/tar.gz/56809e5282f104c5c8b570e7c2996cdc352d94f1
    URL_HASH SHA256=380f4916652cf9bf3da080ecf2b3b6069156578615beb6008cc2c8b1cb938294
  )
  FetchContent_MakeAvailable(catch2)
endif()
