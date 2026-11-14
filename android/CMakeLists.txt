# piccolo for Android: native part (libmain.so).
#
# Step 1 of the port: SDL3 + a minimal main that opens the window and logs what
# Android hands us (window size in pixels, display mode). The real piccolo
# (frontend_libretro, Vulkan, the retro UI) comes in later steps.
#
# Configure with the toolchain next to this file; build-apk.sh does it all:
#   cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/android-toolchain.cmake

cmake_minimum_required(VERSION 3.25)

project(piccolo_android LANGUAGES C CXX)

if(NOT ANDROID)
    message(FATAL_ERROR "Configure with -DCMAKE_TOOLCHAIN_FILE=cmake/android-toolchain.cmake")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# piccolo is a submodule of multitap: deps/ is three levels up.
set(MULTITAP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
if(NOT EXISTS "${MULTITAP_ROOT}/deps/SDL/CMakeLists.txt")
    message(FATAL_ERROR "deps/SDL not found at ${MULTITAP_ROOT}/deps/SDL (piccolo must be inside multitap)")
endif()

# SDL3 as a static library inside libmain.so: one .so in the APK. SDL's Java
# side (SDLActivity) is compiled by build-apk.sh, not by SDL's CMake.
set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
set(SDL_ANDROID_JAR  OFF CACHE BOOL "" FORCE)
add_subdirectory("${MULTITAP_ROOT}/deps/SDL" sdl EXCLUDE_FROM_ALL SYSTEM)

# SDLActivity loads "libmain.so" and calls SDL_main in it.
add_library(main SHARED
    src/android_main.cpp
)
target_link_libraries(main PRIVATE SDL3::SDL3-static android log vulkan)

# Same warnings as multitap_set_project_flags() in multitap's root CMakeLists.
target_compile_options(main PRIVATE
    -Wall -Wextra -Wpedantic
    -Wconversion -Wsign-conversion -Wshadow
    -Wold-style-cast -Wcast-qual -Wnull-dereference
    -Wno-missing-field-initializers
    -fno-exceptions -fno-rtti
)
