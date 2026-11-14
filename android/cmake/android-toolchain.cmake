# CMake toolchain for building piccolo for Android (arm64-v8a) with the
# distro's clang instead of the NDK's.
#
# Why: Google only ships the NDK's compiler for x86_64 hosts, and the main
# development machine is Linux arm64. From the NDK we only use files that are
# the same on every host: the sysroot (Bionic headers and libraries, libc++)
# and compiler-rt's builtins library. Any recent clang can compile for
# aarch64-linux-android with those.
#
# Variables (pass with -D, or set in the environment):
#   ANDROID_NDK_ROOT     the unpacked NDK (default ~/Android/ndk/android-ndk-r30)
#   PICCOLO_LLVM_ROOT    the LLVM install with clang + lld (default /usr/lib/llvm-21)
#   PICCOLO_ANDROID_API  minimum Android API level (default 28, Android 9)

if(NOT ANDROID_NDK_ROOT)
    if(DEFINED ENV{ANDROID_NDK_ROOT})
        set(ANDROID_NDK_ROOT "$ENV{ANDROID_NDK_ROOT}")
    else()
        set(ANDROID_NDK_ROOT "$ENV{HOME}/Android/ndk/android-ndk-r30")
    endif()
endif()
if(NOT PICCOLO_LLVM_ROOT)
    set(PICCOLO_LLVM_ROOT "/usr/lib/llvm-21")
endif()
if(NOT PICCOLO_ANDROID_API)
    # 28 (Android 9): its libvulkan.so exports the Vulkan 1.1 functions
    # vk_context calls (vkEnumerateInstanceVersion, vkGetPhysicalDeviceFeatures2).
    # The Shield TV is on 30.
    set(PICCOLO_ANDROID_API 28)
endif()

# Try-compile projects run this file again in their own folder: hand the
# variables down.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ANDROID_NDK_ROOT PICCOLO_LLVM_ROOT PICCOLO_ANDROID_API)

# The NDK keeps the sysroot inside the (x86_64) prebuilt toolchain folder.
# Only the files are used, never the binaries next to them.
set(_ndk_prebuilt "${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/linux-x86_64")
if(NOT EXISTS "${_ndk_prebuilt}/sysroot/usr/include/android/api-level.h")
    message(FATAL_ERROR "No Android NDK sysroot under ${_ndk_prebuilt}. Set ANDROID_NDK_ROOT.")
endif()
if(NOT EXISTS "${PICCOLO_LLVM_ROOT}/bin/clang")
    message(FATAL_ERROR "No clang in ${PICCOLO_LLVM_ROOT}/bin. Set PICCOLO_LLVM_ROOT.")
endif()

# Plain "Linux" would make CMake and SDL treat this as desktop Linux. "Android"
# would make CMake look for the NDK's own (x86_64) compiler. So: CMake's
# generic Android support is skipped and we say ourselves what Android means.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(ANDROID TRUE)
set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)

set(_target "aarch64-linux-android${PICCOLO_ANDROID_API}")

set(CMAKE_C_COMPILER   "${PICCOLO_LLVM_ROOT}/bin/clang")
set(CMAKE_CXX_COMPILER "${PICCOLO_LLVM_ROOT}/bin/clang++")
set(CMAKE_ASM_COMPILER "${PICCOLO_LLVM_ROOT}/bin/clang")
set(CMAKE_C_COMPILER_TARGET   "${_target}")
set(CMAKE_CXX_COMPILER_TARGET "${_target}")
set(CMAKE_ASM_COMPILER_TARGET "${_target}")
set(CMAKE_AR     "${PICCOLO_LLVM_ROOT}/bin/llvm-ar"     CACHE FILEPATH "")
set(CMAKE_RANLIB "${PICCOLO_LLVM_ROOT}/bin/llvm-ranlib" CACHE FILEPATH "")
set(CMAKE_STRIP  "${PICCOLO_LLVM_ROOT}/bin/llvm-strip"  CACHE FILEPATH "")

set(CMAKE_SYSROOT "${_ndk_prebuilt}/sysroot")

# clang links compiler-rt's builtins and libunwind from its "resource
# directory", and the distro's clang has no Android build of them. So we make
# a resource directory of our own: the distro clang's headers (they must match
# the compiler) plus the NDK's Android runtime libraries. Both are links,
# nothing is copied.
file(GLOB _ndk_clang_dirs "${_ndk_prebuilt}/lib/clang/*")
list(GET _ndk_clang_dirs 0 _ndk_clang_dir)
file(GLOB _llvm_clang_dirs "${PICCOLO_LLVM_ROOT}/lib/clang/*")
list(GET _llvm_clang_dirs 0 _llvm_clang_dir)
set(_resource_dir "${CMAKE_BINARY_DIR}/clang-resource")
file(MAKE_DIRECTORY "${_resource_dir}")
if(NOT IS_SYMLINK "${_resource_dir}/include")
    file(CREATE_LINK "${_llvm_clang_dir}/include" "${_resource_dir}/include" SYMBOLIC)
endif()
if(NOT IS_SYMLINK "${_resource_dir}/lib")
    file(CREATE_LINK "${_ndk_clang_dir}/lib" "${_resource_dir}/lib" SYMBOLIC)
endif()

# Flags the NDK's own toolchain file uses for arm64, minus the ones newer clang
# already defaults to for Android.
set(_common_flags "-resource-dir=${_resource_dir} -fPIC -ffunction-sections -fdata-sections")
set(CMAKE_C_FLAGS_INIT   "${_common_flags}")
set(CMAKE_CXX_FLAGS_INIT "${_common_flags}")
set(CMAKE_ASM_FLAGS_INIT "${_common_flags}")

# Link with lld, compiler-rt and libc++ as a static library (one .so per app,
# nothing else to ship). Bionic needs 16 KB page alignment on new devices.
set(_link_flags "-fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind -static-libstdc++ -Wl,--build-id=sha1 -Wl,--no-undefined -Wl,-z,max-page-size=16384 -Wl,--gc-sections")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_link_flags}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_link_flags}")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${_link_flags}")

# Only look for libraries and headers in the sysroot, programs on the host.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
# Bionic's per-API libraries (libandroid.so, libvulkan.so, ...) live in
# usr/lib/<triple>/<api>.
set(CMAKE_LIBRARY_ARCHITECTURE "aarch64-linux-android")
set(CMAKE_SYSTEM_LIBRARY_PATH "/usr/lib/aarch64-linux-android/${PICCOLO_ANDROID_API}")
