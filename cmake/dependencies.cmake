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
FetchContent_Declare(tinysoundfont
  URL https://codeload.github.com/schellingb/TinySoundFont/tar.gz/853a0a171759f1ddba0de1442133a75912bbeffa
  URL_HASH SHA256=76c356df524f71f2a34f67fa0cb2cb946036fe7a65d2930a849569c5e742fa57
  SOURCE_SUBDIR unused_upstream_build
)
set(libmt32emu_SHARED OFF CACHE BOOL "Link Munt into the game")
FetchContent_Declare(munt
  URL https://codeload.github.com/munt/munt/tar.gz/5ee078a761d35cdd3d27000b551c12d11df6a60b
  URL_HASH SHA256=f2c0885a6a9b40329019424afddf800d53b4a85b2f8d23658a246d2883201b0c
  SOURCE_SUBDIR mt32emu
  EXCLUDE_FROM_ALL
)
FetchContent_Declare(nuked_sc55
  URL https://codeload.github.com/jcmoyer/Nuked-SC55/tar.gz/f3464753f64a7da5f5fd3a96fd72197628369589
  URL_HASH SHA256=241cf3ebc1022ce5a519d42b6c318e3c8a8359994e7598227d5e4af050542a76
  SOURCE_SUBDIR unused_upstream_build
)
FetchContent_MakeAvailable(nuked_sc55)
FetchContent_MakeAvailable(glfw miniaudio nuked_opl3 dosbox_opl tinysoundfont munt)

if(BUILD_TESTING)
  FetchContent_Declare(catch2
    URL https://codeload.github.com/catchorg/Catch2/tar.gz/56809e5282f104c5c8b570e7c2996cdc352d94f1
    URL_HASH SHA256=380f4916652cf9bf3da080ecf2b3b6069156578615beb6008cc2c8b1cb938294
  )
  FetchContent_MakeAvailable(catch2)
endif()

set(UNICORN_ARCH x86 CACHE STRING "Only the AWE32 driver's processor")
set(UNICORN_BUILD_TESTS OFF CACHE BOOL "Disable emulator tests")
set(UNICORN_INSTALL OFF CACHE BOOL "Do not install emulator tools")
FetchContent_Declare(unicorn
  URL https://codeload.github.com/unicorn-engine/unicorn/tar.gz/refs/tags/2.1.4
  URL_HASH SHA256=ea8863f095a0136388694e5a6063afd9bb7650e30243dd6251af59c5ce5601f4
  EXCLUDE_FROM_ALL
)
FetchContent_Declare(dosbox_emu8k
  URL https://codeload.github.com/joncampbell123/dosbox-x/tar.gz/b60936a716c72e0978e940f813bab2cd40d43868
  URL_HASH SHA256=1298f0a2500d928dc13f68ee029232b05acea6288d0c808298abdb7e384d9b2d
  SOURCE_SUBDIR unused_upstream_build
)
FetchContent_MakeAvailable(unicorn dosbox_emu8k)

# Native audio drivers must remain real-time when the game itself is a Debug build.
foreach(backend IN ITEMS unicorn unicorn-common x86_64-softmmu)
  target_compile_options(${backend} PRIVATE $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-O2>)
endforeach()
