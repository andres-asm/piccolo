#!/bin/sh
# Builds SameBoy's libretro core for the browser, as a static archive that
# web/build.sh links into piccolo.wasm (one wasm per core, docs/wasm.md).
#
#   web/build-core.sh   -> web/build/cores/sameboy_libretro_emscripten.a
#
# The same recipe as multitap-org/cores (cores/sameboy/core.cfg + build.sh):
# the source at the commit pinned there, boot ROMs assembled for this machine
# with rgbds, then SameBoy's own libretro Makefile, here with emcc. It lives
# here until the cores repository has a web platform.
#
# Needs: Emscripten in the environment (see web/build.sh), git, a C compiler
# for this machine, and rgbds (rgbasm & co.): on PATH, or RGBDS=<its folder>/.

set -eu

here=$(cd "$(dirname "$0")" && pwd)
out="$here/build/cores"
src="$out/src/sameboy"

# multitap-org/cores cores/sameboy/core.cfg (MIT).
repo=https://github.com/LIJI32/SameBoy.git
commit=ea667ad79aa28409f034b1fd6da7304e1844750d   # tag v1.0.5

if ! command -v emmake >/dev/null 2>&1; then
    echo "web/build-core.sh: emmake not found; source emsdk_env.sh first (see web/build.sh)" >&2
    exit 1
fi
RGBDS=${RGBDS:-}
if [ -z "$RGBDS" ] && ! command -v rgbasm >/dev/null 2>&1; then
    echo "web/build-core.sh: rgbds not found; put rgbasm on PATH or set RGBDS=<its folder>/" >&2
    exit 1
fi
HOST_CC=${HOST_CC:-cc}

# The source at the pinned commit (fetched once).
if [ "$(git -C "$src" rev-parse HEAD 2>/dev/null || true)" != "$commit" ]; then
    rm -rf "$src"
    mkdir -p "$src"
    git -C "$src" init -q
    git -C "$src" fetch -q --depth 1 "$repo" "$commit"
    git -C "$src" checkout -q FETCH_HEAD
fi

# Boot ROMs, for this machine (rgbds and SameBoy's logo compressor run here).
env -u CFLAGS -u CXXFLAGS -u LDFLAGS -u CC -u CXX -u AR \
    make -C "$src" bootroms RGBDS="$RGBDS" CC="$HOST_CC" NATIVE_CC="$HOST_CC" CONF=release

# The core: SameBoy's emscripten platform, as a static archive. -pthread: every
# object in a threaded wasm must be built with atomics (web/build.sh).
# Always from clean: the Makefile doesn't notice changed flags.
jobs=$(nproc 2>/dev/null || echo 4)
rm -rf "$src/build/obj" "$src/build/bin/sameboy_libretro_emscripten.bc"
# CFLAGS from the environment (the Makefile adds its own to it; on make's
# command line it would replace them).
CFLAGS="-pthread" emmake make -C "$src/libretro" platform=emscripten STATIC_LINKING=1 AR=emar \
    BOOTROMS_DIR="$src/build/bin/BootROMs" BIN="$src/build/bin" -j"$jobs"

mkdir -p "$out"
cp "$src/build/bin/sameboy_libretro_emscripten.bc" "$out/sameboy_libretro_emscripten.a"
cp "$src/LICENSE" "$out/sameboy-LICENSE"
echo "web/build-core.sh: $out/sameboy_libretro_emscripten.a"
