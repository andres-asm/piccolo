#!/bin/sh
# Builds piccolo for the browser (WebAssembly, Emscripten + SDL3, WebGL 2
# through the OpenGL ES path). See docs/wasm.md.
#
#   web/build-core.sh       -> web/build/cores/sameboy_libretro_emscripten.a
#   web/build.sh            -> web/build/wasm/piccolo.{js,wasm}, SameBoy linked in
#
# Needs Emscripten in the environment (emcc on PATH), the version CI pins:
#   git clone https://github.com/emscripten-core/emsdk ~/emsdk
#   ~/emsdk/emsdk install 6.0.13 && ~/emsdk/emsdk activate 6.0.13
#   . ~/emsdk/emsdk_env.sh
#
# One build, with pthreads (SharedArrayBuffer): the page needs the COOP/COEP
# headers, or coi-serviceworker where the server can't send them.
#
# Steps: the strings generator for this machine (piccolo's build runs it, and
# a cross build can't run what it builds; same as android/build-apk.sh), then
# piccolo with emcmake.
#
# Overridable from the environment:
#   BUILD_TYPE   CMake build type (default Release)
#   CORE_ARCHIVE the core's static archive (default: SameBoy's, from web/build-core.sh)
#   CORE_FILE    the file name it answers to (default sameboy_libretro.so)

set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
build="$here/build"
host_build="$build/host"
wasm_build="$build/wasm"
BUILD_TYPE=${BUILD_TYPE:-Release}
CORE_ARCHIVE=${CORE_ARCHIVE:-$build/cores/sameboy_libretro_emscripten.a}
CORE_FILE=${CORE_FILE:-sameboy_libretro.so}
if [ ! -f "$CORE_ARCHIVE" ]; then
    echo "web/build.sh: no core archive at $CORE_ARCHIVE (run web/build-core.sh first)" >&2
    exit 1
fi

if ! command -v emcmake >/dev/null 2>&1; then
    echo "web/build.sh: emcmake not found; source emsdk_env.sh first (see the top of this script)" >&2
    exit 1
fi

# The strings generator, built for this machine.
cmake -S "$root" -B "$host_build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DMULTITAP_BUILD_TESTS=OFF \
    -DMULTITAP_GPU_VULKAN=OFF -DMULTITAP_PICCOLO_DIR= >/dev/null
cmake --build "$host_build" --target multitap_strings_generator
generator=$(find "$host_build" -type f -name multitap_strings_generator | head -n 1)

# Vulkan's headers only (no library): frontends-common's headers still name
# Vulkan types with Vulkan off (its *_none.cpp stubs), and Emscripten has no
# vulkan.h. The system's headers (Vulkan-Headers, which the desktop build
# needs anyway), seen through a folder that holds nothing else, so no other
# system header leaks into the wasm build.
vulkan_headers=${VULKAN_HEADERS:-/usr/include}
if [ ! -f "$vulkan_headers/vulkan/vulkan.h" ]; then
    echo "web/build.sh: no vulkan/vulkan.h in $vulkan_headers (install the Vulkan headers or set VULKAN_HEADERS)" >&2
    exit 1
fi
vulkan_include="$build/vulkan-include"
mkdir -p "$vulkan_include"
ln -sfn "$vulkan_headers/vulkan" "$vulkan_include/vulkan"
ln -sfn "$vulkan_headers/vk_video" "$vulkan_include/vk_video"

# piccolo. -pthread on every file: a threaded wasm needs all its objects
# built with atomics (SDL's own build adds it with SDL_PTHREADS).
emcmake cmake -S "$root" -B "$wasm_build" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_C_FLAGS="-pthread" -DCMAKE_CXX_FLAGS="-pthread -isystem $vulkan_include" \
    -DSDL_PTHREADS=ON \
    -DMULTITAP_GPU_VULKAN=OFF -DMULTITAP_BUILD_TESTS=OFF -DMULTITAP_PICCOLO_DIR= \
    -DMULTITAP_STRINGS_GENERATOR="$generator" \
    -DPICCOLO_LINKED_CORE="$CORE_ARCHIVE" -DPICCOLO_LINKED_CORE_FILE="$CORE_FILE"
cmake --build "$wasm_build" --target piccolo

echo "web/build.sh: $wasm_build/piccolo.js + piccolo.wasm"
