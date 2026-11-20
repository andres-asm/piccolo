#!/bin/sh
# Builds piccolo's Android APK without Gradle (Gradle's Android plugin only
# ships aapt2 for x86_64; the development machine is Linux arm64). The same
# steps as piccoletto's android/build-apk.sh.
#
#   android/build-apk.sh           -> android/build/piccolo.apk
#   android/build-apk.sh --legacy  -> android/build/piccolo-legacy.apk: Android 5+ on
#       32-bit ARM, OpenGL ES 2 only (the SHIELD Portable, Tegra 4). No Vulkan in
#       it (Android 5 has no libvulkan.so), so it's a separate build: armeabi-v7a,
#       minSdk 21, MULTITAP_GPU_VULKAN=OFF, MULTITAP_GL_ES2=ON. Its own package
#       (site.multitap.piccolo.legacy), name ("piccolo legacy"), banner (a
#       LEGACY tag, android/res-legacy) and folder (/sdcard/piccolo-legacy): it
#       installs next to the main APK, on a device that takes both.
#   adb install -r android/build/piccolo.apk
#   adb logcat -s piccolo SDL
#
# Steps: the strings generator for this machine (piccolo's build runs it),
# native library (piccolo's own CMakeLists with hosts-common's Android
# toolchain: libmain.so, plus Test libretro when deps/test-libretro is there),
# Java (javac), dex (d8), manifest, resources and assets (aapt2: icon, TV
# banner, lang/ layouts/ themes/), then the native libraries and dex go into
# the APK, which is aligned (zipalign) and signed (apksigner).
#
# Tools, overridable from the environment:
#   ANDROID_NDK_ROOT  NDK, only its sysroot is used (default ~/Android/ndk/android-ndk-r30)
#   ANDROID_JAR       android.jar that Java compiles against (default /usr/lib/android-sdk/platforms/android-35/android.jar)
#   AAPT_ANDROID_JAR  android.jar that aapt2 links the manifest against (default .../android-32/android.jar):
#                     Debian's aapt2 (2.19) can't read the resource table of API 33+ jars.
#   D8_JAR            the r8/d8 jar (default ~/Android/r8/r8.jar)
#   JAVA_HOME         a JDK for javac/java (default /usr/lib/jvm/java-21-openjdk-arm64)
#   BUILD_TYPE        CMake build type (default Debug)
#   MULTITAP_LLVM_ROOT clang, lld and llvm-ar for Android (default /usr/lib/llvm-21; CI: the NDK's own)
#   CARGO             cargo with the aarch64-linux-android target (default ~/.cargo/bin/cargo, then cargo)
#                     (rustup target add aarch64-linux-android), for librashader
#
# Release or debug, decided by the key:
#   PICCOLO_KEYSTORE set: a release build, signed with the release key in that
#     keystore (PICCOLO_KEY_ALIAS, PICCOLO_KEYSTORE_PASS; the key's password is
#     the same). Package site.multitap.piccolo, named "piccolo". CI builds.
#   Not set: a debug build, signed with the standard Android debug key. Package
#     site.multitap.piccolo.debug, named "piccolo debug", banner with a DEBUG
#     tag (android/res-debug). It installs next to a release build instead of
#     clashing with it (Android refuses an update signed by another key).
#     With --legacy: site.multitap.piccolo.legacy.debug, "piccolo legacy
#     debug", both tags (android/res-legacy-debug).
# The folder follows the package (PiccoloActivity.java): /sdcard/piccolo,
# piccolo-debug, piccolo-legacy, piccolo-legacy-debug.

set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
build="$here/build"
native="$build/native"
host_build="$here/build-host"
apk_dir="$build/apk"

ANDROID_JAR=${ANDROID_JAR:-/usr/lib/android-sdk/platforms/android-35/android.jar}
AAPT_ANDROID_JAR=${AAPT_ANDROID_JAR:-/usr/lib/android-sdk/platforms/android-32/android.jar}
D8_JAR=${D8_JAR:-$HOME/Android/r8/r8.jar}
JAVA_HOME=${JAVA_HOME:-/usr/lib/jvm/java-21-openjdk-arm64}
BUILD_TYPE=${BUILD_TYPE:-Debug}
ANDROID_NDK_ROOT=${ANDROID_NDK_ROOT:-$HOME/Android/ndk/android-ndk-r30}
MULTITAP_LLVM_ROOT=${MULTITAP_LLVM_ROOT:-/usr/lib/llvm-21}
export ANDROID_NDK_ROOT MULTITAP_LLVM_ROOT   # the CMake toolchain reads them too
# rustup's cargo (it has the Android target) before a distro one, which may not.
if [ -z "${CARGO:-}" ]; then
    if [ -x "$HOME/.cargo/bin/cargo" ]; then
        CARGO="$HOME/.cargo/bin/cargo"
    else
        CARGO=cargo
    fi
fi
# The main APK: arm64, Android 9+ (its libvulkan exports Vulkan 1.1), Vulkan
# with an OpenGL ES 3 fallback. --legacy: see the top of this file.
legacy=0
if [ "${1:-}" = "--legacy" ]; then
    legacy=1
fi
if [ "$legacy" = 1 ]; then
    MIN_API=21
    # PICCOLO_LEGACY_ABI=arm64-v8a: the same GLES 2 build for 64-bit devices
    # (to tell a 32-bit driver's problem from ours on a device that runs both).
    ABI=${PICCOLO_LEGACY_ABI:-armeabi-v7a}
    native="$build/native-legacy-$ABI"
    apk_name=piccolo-legacy.apk
    gpu_flags="-DMULTITAP_GPU_VULKAN=OFF -DMULTITAP_GL_ES2=ON"
else
    MIN_API=28
    ABI=arm64-v8a
    apk_name=piccolo.apk
    gpu_flags=""
fi
if [ -n "${PICCOLO_KEYSTORE:-}" ]; then
    variant=release
else
    variant=debug
fi

for f in "$ANDROID_JAR" "$AAPT_ANDROID_JAR" "$D8_JAR" "$JAVA_HOME/bin/javac"; do
    if [ ! -e "$f" ]; then
        echo "missing: $f" >&2
        exit 1
    fi
done

echo "== build tools for this machine"
# piccolo's build runs multitap_strings_generator (the string IDs); a cross
# build can't run what it builds, so it's built for this machine first.
# SDL_UNIX_CONSOLE_BUILD: nothing here opens a window, so SDL needn't find
# X11 / Wayland development packages on the build machine (CI runners).
cmake -S "$root" -B "$host_build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DMULTITAP_BUILD_TESTS=OFF \
    -DMULTITAP_PICCOLO_DIR= -DSDL_UNIX_CONSOLE_BUILD=ON >/dev/null
cmake --build "$host_build" --target multitap_strings_generator
generator=$(find "$host_build" -type f -name multitap_strings_generator | head -n 1)

echo "== native library"
# piccolo's own CMakeLists: with ANDROID set (the toolchain) it builds
# libmain.so instead of the program.
# shellcheck disable=SC2086 # $gpu_flags is a list of words without spaces
cmake -S "$root" -B "$native" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_TOOLCHAIN_FILE="$root/deps/common/hosts-common/cmake/android-toolchain.cmake" \
    -DMULTITAP_ANDROID_ABI="$ABI" -DMULTITAP_ANDROID_API="$MIN_API" $gpu_flags \
    -DMULTITAP_BUILD_TESTS=OFF -DMULTITAP_PICCOLO_DIR= -DMULTITAP_STRINGS_GENERATOR="$generator" >/dev/null
cmake --build "$native" --target piccolo

# Version: piccolo's (the build's piccolo_version.h) as versionName; the
# number of commits as versionCode, which only grows on one branch.
echo "== librashader"
# Shader presets (frontends-common shader_chain): librashader's C API with the
# Vulkan runtime only, as in the Flatpak, built by cargo for Android. piccolo
# loads it at run time (lib/arm64-v8a/librashader.so: dlopen finds a bare
# name among the APK's libraries); without it presets are off. cargo's output
# goes to android/build (deps/librashader is read-only). Its C++ crates
# (glslang, SPIRV-Cross) and the link use the same clang + NDK sysroot as
# libmain.so, through small wrapper scripts; libc++ is linked in statically.
librashader=""
# Not in the legacy APK: it draws with OpenGL ES (librashader here is its Vulkan runtime).
if [ "$legacy" = 0 ] && [ -f "$root/deps/librashader/Cargo.toml" ]; then
    rust_build="$build/librashader"
    mkdir -p "$rust_build"
    ndk_prebuilt="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64"
    target_flags="--target=aarch64-linux-android$MIN_API --sysroot=$ndk_prebuilt/sysroot -resource-dir=$native/clang-resource"
    # The linker's name ends in "clang": rustc tells the kind of linker by its
    # name, and passes linker options as -Wl,... only to a compiler driver.
    # Our flags go last: cc-rs passes its own --target without the API level,
    # which the NDK's headers refuse; the last --target wins.
    printf '#!/bin/sh\nexec "%s/bin/clang" "$@" %s\n' "$MULTITAP_LLVM_ROOT" "$target_flags" > "$rust_build/cc"
    printf '#!/bin/sh\nexec "%s/bin/clang++" "$@" %s\n' "$MULTITAP_LLVM_ROOT" "$target_flags" > "$rust_build/cxx"
    printf '#!/bin/sh\nexec "%s/bin/clang++" "$@" %s %s\n' "$MULTITAP_LLVM_ROOT" "$target_flags" \
        "-fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind -Wl,-z,max-page-size=16384 -l:libc++_static.a -l:libc++abi.a" > "$rust_build/linker-clang"
    chmod +x "$rust_build/cc" "$rust_build/cxx" "$rust_build/linker-clang"
    # CXXSTDLIB empty: the cc crate adds no libc++ of its own. rustc links
    # with -nodefaultlibs, so the link wrapper names the NDK's static libc++
    # and libc++abi itself (nothing to ship next to it, no libc++_shared.so).
    # cargo's own folder first in PATH: rustup's cargo would otherwise run a
    # distro rustc found first, which has no Android standard library.
    (cd "$root/deps/librashader" && \
        PATH="$(dirname "$(command -v "$CARGO")"):$PATH" \
        CARGO_TARGET_DIR="$rust_build/target" \
        CC_aarch64_linux_android="$rust_build/cc" CXX_aarch64_linux_android="$rust_build/cxx" \
        AR_aarch64_linux_android="$MULTITAP_LLVM_ROOT/bin/llvm-ar" CXXSTDLIB_aarch64_linux_android= \
        CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$rust_build/linker-clang" \
        "$CARGO" build -p librashader-capi --release --locked --no-default-features --features runtime-vulkan \
            --target aarch64-linux-android)
    librashader="$rust_build/librashader.so"
    "$MULTITAP_LLVM_ROOT/bin/llvm-strip" --strip-unneeded -o "$librashader" \
        "$rust_build/target/aarch64-linux-android/release/liblibrashader_capi.so"
else
    echo "legacy APK or deps/librashader not checked out: no librashader in this APK"
fi

version_name=$(sed -n 's/^#define PICCOLO_VERSION "\(.*\)"$/\1/p' "$native/generated/piccolo_version.h")
version_code=$(git -C "$root" rev-list --count HEAD)
echo "piccolo $version_name (versionCode $version_code, $variant, $ABI, minSdk $MIN_API)"

echo "== java"
rm -rf "$apk_dir"
mkdir -p "$apk_dir/classes" "$apk_dir/dex" "$apk_dir/lib/$ABI" "$apk_dir/assets"
# SDL's Java side straight from the submodule, plus our activities.
find "$root/deps/SDL/android-project/app/src/main/java" "$here/java" -name '*.java' > "$apk_dir/sources.txt"
# Java 8 bytecode against android.jar as the boot class path (not the JDK's
# classes): only Android APIs can be used. d8 handles the rest.
"$JAVA_HOME/bin/javac" -nowarn -Xlint:-options -source 8 -target 8 -bootclasspath "$ANDROID_JAR" \
    -d "$apk_dir/classes" @"$apk_dir/sources.txt"

echo "== dex"
find "$apk_dir/classes" -name '*.class' > "$apk_dir/classes.txt"
"$JAVA_HOME/bin/java" -cp "$D8_JAR" com.android.tools.r8.D8 --release --min-api "$MIN_API" \
    --lib "$ANDROID_JAR" --output "$apk_dir/dex" @"$apk_dir/classes.txt"

echo "== package"
# Assets: on Android SDL's base path is the APK's assets, so piccolo finds
# lang/ there like next to the program; layouts/ and themes/ are copied out at
# start (src/bundled_files_android.cpp).
cp -r "$root/lang" "$root/layouts" "$root/themes" "$root/shaders" "$apk_dir/assets/"
# The curated shader presets (deps/piccolo-shaders, optional), where the
# desktop build puts them: shaders/presets. Copied out at start like the rest.
if [ -e "$root/deps/piccolo-shaders/LICENSES" ]; then
    cmake -DSOURCE="$root/deps/piccolo-shaders" -DDESTINATION="$apk_dir/assets/shaders/presets" \
        -P "$root/cmake/copy_presets.cmake"
fi
if [ -n "$librashader" ]; then
    mkdir -p "$apk_dir/assets/licenses"
    cp "$root/deps/librashader/LICENSE.md" "$apk_dir/assets/licenses/librashader-LICENSE.md"
fi
# Resources: the name, launcher icon and TV banner (android/res, exported
# from assets/branding). Debug builds lay android/res-debug over them (the
# later -R wins) and rename the package; aapt2 keeps the activities' class
# names pointing at the Java package, which doesn't change.
# --replace-version: the version flags only fill in a missing version otherwise.
# The legacy APK's manifest: minSdk 21 and no Vulkan requirement (the
# Play-style uses-feature line would hide it from devices without Vulkan).
manifest="$here/AndroidManifest.xml"
if [ "$legacy" = 1 ]; then
    manifest="$apk_dir/AndroidManifest.xml"
    sed -e 's/android:minSdkVersion="28"/android:minSdkVersion="21"/' \
        -e '/android.hardware.vulkan.version/d' "$here/AndroidManifest.xml" > "$manifest"
    grep -q 'android:minSdkVersion="21"' "$manifest" || { echo "legacy manifest: minSdkVersion not set" >&2; exit 1; }
fi
aapt2 compile --dir "$here/res" -o "$apk_dir/res.zip"
# The overlay (name, banner) and the package for this build.
package=site.multitap.piccolo
overlay=""
if [ "$legacy" = 1 ]; then
    package="$package.legacy"
    overlay=res-legacy
fi
if [ "$variant" = debug ]; then
    package="$package.debug"
    if [ "$legacy" = 1 ]; then
        overlay=res-legacy-debug
    else
        overlay=res-debug
    fi
fi
variant_flags=""
if [ -n "$overlay" ]; then
    aapt2 compile --dir "$here/$overlay" -o "$apk_dir/$overlay.zip"
    variant_flags="-R $apk_dir/$overlay.zip"
fi
if [ "$package" != site.multitap.piccolo ]; then
    variant_flags="$variant_flags --rename-manifest-package $package"
fi
# shellcheck disable=SC2086 # $variant_flags is a list of words without spaces
aapt2 link --manifest "$manifest" -I "$AAPT_ANDROID_JAR" -R "$apk_dir/res.zip" $variant_flags \
    --auto-add-overlay -A "$apk_dir/assets" --replace-version --version-name "$version_name" --version-code "$version_code" \
    -o "$apk_dir/unaligned.apk"
libs="lib/$ABI/libmain.so"
cp "$native/libmain.so" "$apk_dir/lib/$ABI/libmain.so"
# Android only extracts lib/<abi>/lib*.so when installing, hence the "lib"
# prefix for the core (bundled_files_install_cores takes it off again).
if [ -f "$native/test_libretro.so" ]; then
    cp "$native/test_libretro.so" "$apk_dir/lib/$ABI/libtest_libretro.so"
    libs="$libs lib/$ABI/libtest_libretro.so"
fi
if [ -n "$librashader" ]; then
    cp "$librashader" "$apk_dir/lib/arm64-v8a/librashader.so"
    libs="$libs lib/arm64-v8a/librashader.so"
fi
# Paths inside the APK are relative to the folder zip runs in.
(cd "$apk_dir/dex" && zip -q "$apk_dir/unaligned.apk" classes.dex)
# shellcheck disable=SC2086 # $libs is a list of paths without spaces
(cd "$apk_dir" && zip -q "$apk_dir/unaligned.apk" $libs)
zipalign -f -p 4 "$apk_dir/unaligned.apk" "$apk_dir/aligned.apk"

echo "== sign"
if [ -n "${PICCOLO_KEYSTORE:-}" ]; then
    JAVA_HOME="$JAVA_HOME" apksigner sign --ks "$PICCOLO_KEYSTORE" --ks-key-alias "${PICCOLO_KEY_ALIAS:?}" \
        --ks-pass env:PICCOLO_KEYSTORE_PASS --key-pass env:PICCOLO_KEYSTORE_PASS \
        --out "$build/$apk_name" "$apk_dir/aligned.apk"
else
    # The standard Android debug key, the one Android Studio would create
    # (shared with piccoletto's builds).
    keystore="$HOME/.android/debug.keystore"
    if [ ! -f "$keystore" ]; then
        mkdir -p "$HOME/.android"
        "$JAVA_HOME/bin/keytool" -genkeypair -keystore "$keystore" -storepass android -keypass android \
            -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Android Debug,O=Android,C=US"
    fi
    JAVA_HOME="$JAVA_HOME" apksigner sign --ks "$keystore" --ks-pass pass:android --ks-key-alias androiddebugkey \
        --out "$build/$apk_name" "$apk_dir/aligned.apk"
fi
apksigner verify "$build/$apk_name"
echo "built $build/$apk_name"
