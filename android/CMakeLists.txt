# piccolo for Android: the native part (libmain.so), built from multitap's
# root CMakeLists.txt when ANDROID is set. build-apk.sh does it all:
#   cmake -S <multitap> -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=<this folder>/cmake/android-toolchain.cmake
#         -DMULTITAP_BUILD_TESTS=OFF
#
# Step 2 of the port: app_shell from frontend_common (SDL window, Vulkan,
# swapchain, renderer, Dear ImGui) draws a test screen. The real piccolo
# (frontend_libretro, cores, the retro UI) comes in later steps.

# SDLActivity loads "libmain.so" and calls SDL_main in it. SDL3 is static
# (from the root CMakeLists), so this is the only .so in the APK. SDL's Java
# side (SDLActivity) is compiled by build-apk.sh.
add_library(main SHARED
    src/android_main.cpp
)
target_link_libraries(main PRIVATE frontend_common android log)
# Next to the build folder's root, where build-apk.sh picks it up.
set_target_properties(main PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

multitap_set_project_flags(main)
