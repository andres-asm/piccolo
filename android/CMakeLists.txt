# piccolo for Android: the native part (libmain.so), built from multitap's
# root CMakeLists.txt when ANDROID is set. build-apk.sh does it all:
#   cmake -S <multitap> -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=<this folder>/cmake/android-toolchain.cmake
#         -DMULTITAP_BUILD_TESTS=OFF
#
# Step 3 of the port: a libretro core runs through frontend_libretro's core
# session (video, audio, timing, input), drawn with picture/viewport like
# piccolo on desktop. The core for now: Test libretro, shipped in the APK.
# piccolo's own UI (retro UI, menus, settings) comes in a later step.

# SDLActivity loads "libmain.so" and calls SDL_main in it. SDL3 is static
# (from the root CMakeLists), so this is the only .so of ours besides cores.
# SDL's Java side (SDLActivity) is compiled by build-apk.sh.
add_library(main SHARED
    src/android_main.cpp
)
target_link_libraries(main PRIVATE frontend_libretro android log)
# Next to the build folder's root, where build-apk.sh picks it up.
set_target_properties(main PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# Test libretro goes into the APK too (build-apk.sh): build it with libmain.
if(TARGET test_libretro)
    add_dependencies(main test_libretro)
    set_target_properties(test_libretro PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

multitap_set_project_flags(main)
