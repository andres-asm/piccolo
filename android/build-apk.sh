#!/bin/sh
# Builds piccolo's Android APK without Gradle (Gradle's Android plugin only
# ships aapt2 for x86_64; the development machine is Linux arm64). The same
# steps as piccoletto's android/build-apk.sh.
#
#   android/build-apk.sh      -> android/build/piccolo.apk
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
#
# Release or debug, decided by the key:
#   PICCOLO_KEYSTORE set: a release build, signed with the release key in that
#     keystore (PICCOLO_KEY_ALIAS, PICCOLO_KEYSTORE_PASS; the key's password is
#     the same). Package site.multitap.piccolo, named "piccolo". CI builds.
#   Not set: a debug build, signed with the standard Android debug key. Package
#     site.multitap.piccolo.debug, named "piccolo debug", banner with a DEBUG
#     tag (android/res-debug). It installs next to a release build instead of
#     clashing with it (Android refuses an update signed by another key).
# The debug build's folder is /sdcard/piccolo-debug (PiccoloActivity.java).

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
MIN_API=28
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
cmake -S "$root" -B "$native" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_TOOLCHAIN_FILE="$root/deps/common/hosts-common/cmake/android-toolchain.cmake" \
    -DMULTITAP_BUILD_TESTS=OFF -DMULTITAP_PICCOLO_DIR= -DMULTITAP_STRINGS_GENERATOR="$generator" >/dev/null
cmake --build "$native" --target piccolo

# Version: piccolo's (the build's piccolo_version.h) as versionName; the
# number of commits as versionCode, which only grows on one branch.
version_name=$(sed -n 's/^#define PICCOLO_VERSION "\(.*\)"$/\1/p' "$native/generated/piccolo_version.h")
version_code=$(git -C "$root" rev-list --count HEAD)
echo "piccolo $version_name (versionCode $version_code, $variant)"

echo "== java"
rm -rf "$apk_dir"
mkdir -p "$apk_dir/classes" "$apk_dir/dex" "$apk_dir/lib/arm64-v8a" "$apk_dir/assets"
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
cp -r "$root/lang" "$root/layouts" "$root/themes" "$apk_dir/assets/"
# Resources: the name, launcher icon and TV banner (android/res, exported
# from assets/branding). Debug builds lay android/res-debug over them (the
# later -R wins) and rename the package; aapt2 keeps the activities' class
# names pointing at the Java package, which doesn't change.
# --replace-version: the version flags only fill in a missing version otherwise.
aapt2 compile --dir "$here/res" -o "$apk_dir/res.zip"
if [ "$variant" = debug ]; then
    aapt2 compile --dir "$here/res-debug" -o "$apk_dir/res-debug.zip"
    variant_flags="-R $apk_dir/res-debug.zip --rename-manifest-package site.multitap.piccolo.debug"
else
    variant_flags=""
fi
# shellcheck disable=SC2086 # $variant_flags is a list of words without spaces
aapt2 link --manifest "$here/AndroidManifest.xml" -I "$AAPT_ANDROID_JAR" -R "$apk_dir/res.zip" $variant_flags \
    --auto-add-overlay -A "$apk_dir/assets" --replace-version --version-name "$version_name" --version-code "$version_code" \
    -o "$apk_dir/unaligned.apk"
libs="lib/arm64-v8a/libmain.so"
cp "$native/libmain.so" "$apk_dir/lib/arm64-v8a/libmain.so"
# Android only extracts lib/<abi>/lib*.so when installing, hence the "lib"
# prefix for the core (bundled_files_install_cores takes it off again).
if [ -f "$native/test_libretro.so" ]; then
    cp "$native/test_libretro.so" "$apk_dir/lib/arm64-v8a/libtest_libretro.so"
    libs="$libs lib/arm64-v8a/libtest_libretro.so"
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
        --out "$build/piccolo.apk" "$apk_dir/aligned.apk"
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
        --out "$build/piccolo.apk" "$apk_dir/aligned.apk"
fi
apksigner verify "$build/piccolo.apk"
echo "built $build/piccolo.apk"
