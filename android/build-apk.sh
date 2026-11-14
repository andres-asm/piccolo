#!/bin/sh
# Builds piccolo's Android APK without Gradle (Gradle's Android plugin only
# ships aapt2 for x86_64; the development machine is Linux arm64).
#
#   ./build-apk.sh            debug build -> build/piccolo.apk
#   adb install -r build/piccolo.apk
#   adb logcat -s piccolo SDL
#
# Steps: native library (CMake), Java (javac), dex (d8), resources and
# manifest (aapt2), then the native library and dex go into the APK, which is
# aligned (zipalign) and signed with the debug key (apksigner).
#
# Tools, overridable from the environment:
#   ANDROID_NDK_ROOT  NDK, only its sysroot is used (default ~/Android/ndk/android-ndk-r30)
#   ANDROID_JAR       android.jar that Java compiles against (default /usr/lib/android-sdk/platforms/android-35/android.jar)
#   AAPT_ANDROID_JAR  android.jar that aapt2 links the manifest against (default .../android-32/android.jar):
#                     Debian's aapt2 (2.19) can't read the resource table of API 33+ jars.
#   D8_JAR            the r8/d8 jar (default ~/Android/r8/r8.jar)
#   JAVA_HOME         a JDK for javac/java (default /usr/lib/jvm/java-21-openjdk-arm64)

set -eu

here=$(cd "$(dirname "$0")" && pwd)
multitap_root=$(cd "$here/../../.." && pwd)
build="$here/build"
apk_dir="$build/apk"

ANDROID_JAR=${ANDROID_JAR:-/usr/lib/android-sdk/platforms/android-35/android.jar}
AAPT_ANDROID_JAR=${AAPT_ANDROID_JAR:-/usr/lib/android-sdk/platforms/android-32/android.jar}
D8_JAR=${D8_JAR:-$HOME/Android/r8/r8.jar}
JAVA_HOME=${JAVA_HOME:-/usr/lib/jvm/java-21-openjdk-arm64}
MIN_API=28

for f in "$ANDROID_JAR" "$AAPT_ANDROID_JAR" "$D8_JAR" "$JAVA_HOME/bin/javac"; do
    if [ ! -e "$f" ]; then
        echo "missing: $f" >&2
        exit 1
    fi
done

echo "== native library"
# From multitap's root CMakeLists.txt: it builds only piccolo's libmain.so for Android.
cmake -S "$multitap_root" -B "$build" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$here/cmake/android-toolchain.cmake" \
    -DMULTITAP_BUILD_TESTS=OFF >/dev/null
cmake --build "$build"

echo "== java"
rm -rf "$apk_dir"
mkdir -p "$apk_dir/classes" "$apk_dir/dex" "$apk_dir/lib/arm64-v8a"
# SDL's Java side straight from the submodule, plus our activity.
find "$multitap_root/deps/SDL/android-project/app/src/main/java" "$here/java" -name '*.java' > "$apk_dir/sources.txt"
# Java 8 bytecode against android.jar as the boot class path (not the JDK's
# classes): only Android APIs can be used. d8 handles the rest.
"$JAVA_HOME/bin/javac" -nowarn -Xlint:-options -source 8 -target 8 -bootclasspath "$ANDROID_JAR" \
    -d "$apk_dir/classes" @"$apk_dir/sources.txt"

echo "== dex"
find "$apk_dir/classes" -name '*.class' > "$apk_dir/classes.txt"
"$JAVA_HOME/bin/java" -cp "$D8_JAR" com.android.tools.r8.D8 --debug --min-api "$MIN_API" \
    --lib "$ANDROID_JAR" --output "$apk_dir/dex" @"$apk_dir/classes.txt"

echo "== package"
aapt2 link --manifest "$here/AndroidManifest.xml" -I "$AAPT_ANDROID_JAR" -o "$apk_dir/unaligned.apk"
cp "$build/libmain.so" "$apk_dir/lib/arm64-v8a/libmain.so"
# Paths inside the APK are relative to the folder zip runs in.
(cd "$apk_dir/dex" && zip -q "$apk_dir/unaligned.apk" classes.dex)
(cd "$apk_dir" && zip -q "$apk_dir/unaligned.apk" lib/arm64-v8a/libmain.so)
zipalign -f -p 4 "$apk_dir/unaligned.apk" "$apk_dir/aligned.apk"

echo "== sign"
# The standard Android debug key, the one Android Studio would create.
keystore="$HOME/.android/debug.keystore"
if [ ! -f "$keystore" ]; then
    mkdir -p "$HOME/.android"
    "$JAVA_HOME/bin/keytool" -genkeypair -keystore "$keystore" -storepass android -keypass android \
        -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Android Debug,O=Android,C=US"
fi
JAVA_HOME="$JAVA_HOME" apksigner sign --ks "$keystore" --ks-pass pass:android --ks-key-alias androiddebugkey \
    --out "$build/piccolo.apk" "$apk_dir/aligned.apk"
apksigner verify "$build/piccolo.apk"
echo "built $build/piccolo.apk"
