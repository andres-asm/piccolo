#!/bin/sh
# Builds a libretro core for the browser, as a static archive that
# web/build.sh links into piccolo.wasm (one wasm per core, docs/wasm.md).
#
#   web/build-core.sh [core]   -> web/build/cores/<core>_libretro_emscripten.a
#                                 web/build/cores/<core>-LICENSE
#
# Cores (default sameboy):
#   sameboy   the same recipe as multitap-org/cores (cores/sameboy/core.cfg +
#             build.sh): the source at the commit pinned there, boot ROMs
#             assembled for this machine with rgbds, then SameBoy's own
#             libretro Makefile, here with emcc.
#   snes9x    libretro/snes9x at a pinned commit, its libretro Makefile. Not
#             in multitap-org/cores (non-commercial license: redistribution
#             allowed, free of charge).
# The recipes live here until the cores repository has a web platform.
#
# Needs: Emscripten in the environment (see web/build.sh), git; for SameBoy
# also a C compiler for this machine and rgbds (rgbasm & co.): on PATH, or
# RGBDS=<its folder>/.

set -eu

here=$(cd "$(dirname "$0")" && pwd)
out="$here/build/cores"
core=${1:-sameboy}

case "$core" in
    sameboy)
        repo=https://github.com/LIJI32/SameBoy.git
        commit=ea667ad79aa28409f034b1fd6da7304e1844750d   # tag v1.0.5 (multitap-org/cores, MIT)
        ;;
    snes9x)
        repo=https://github.com/libretro/snes9x.git
        commit=fae2fea08f74180759ef540ee94259213f503480   # master, 2026-11
        ;;
    *)
        echo "web/build-core.sh: unknown core '$core' (sameboy, snes9x)" >&2
        exit 1
        ;;
esac
src="$out/src/$core"

if ! command -v emmake >/dev/null 2>&1; then
    echo "web/build-core.sh: emmake not found; source emsdk_env.sh first (see web/build.sh)" >&2
    exit 1
fi

# The source at the pinned commit (fetched once).
if [ "$(git -C "$src" rev-parse HEAD 2>/dev/null || true)" != "$commit" ]; then
    rm -rf "$src"
    mkdir -p "$src"
    git -C "$src" init -q
    git -C "$src" fetch -q --depth 1 "$repo" "$commit"
    git -C "$src" checkout -q FETCH_HEAD
fi

# -pthread on every object: a threaded wasm needs all its objects built with
# atomics (web/build.sh). CFLAGS / CXXFLAGS from the environment: the
# Makefiles add their own to them (on make's command line they would replace
# them). Always from clean: the Makefiles don't notice changed flags.
jobs=$(nproc 2>/dev/null || echo 4)
mkdir -p "$out"

if [ "$core" = sameboy ]; then
    RGBDS=${RGBDS:-}
    if [ -z "$RGBDS" ] && ! command -v rgbasm >/dev/null 2>&1; then
        echo "web/build-core.sh: rgbds not found; put rgbasm on PATH or set RGBDS=<its folder>/" >&2
        exit 1
    fi
    HOST_CC=${HOST_CC:-cc}
    # Boot ROMs, for this machine (rgbds and SameBoy's logo compressor run here).
    env -u CFLAGS -u CXXFLAGS -u LDFLAGS -u CC -u CXX -u AR \
        make -C "$src" bootroms RGBDS="$RGBDS" CC="$HOST_CC" NATIVE_CC="$HOST_CC" CONF=release
    rm -rf "$src/build/obj" "$src/build/bin/sameboy_libretro_emscripten.bc"
    CFLAGS="-pthread" emmake make -C "$src/libretro" platform=emscripten STATIC_LINKING=1 AR=emar \
        BOOTROMS_DIR="$src/build/bin/BootROMs" BIN="$src/build/bin" -j"$jobs"
    cp "$src/build/bin/sameboy_libretro_emscripten.bc" "$out/sameboy_libretro_emscripten.a"
    cp "$src/LICENSE" "$out/sameboy-LICENSE"
fi

if [ "$core" = snes9x ]; then
    find "$src" -name '*.o' -delete
    rm -f "$src/libretro/snes9x_libretro_emscripten.bc"
    # STATIC_LINKING=0 (on the command line, so it wins over the emscripten
    # platform's =1): with it, the Makefile leaves libretro-common's file
    # streams and VFS out of the archive, for RetroArch's own copy; piccolo
    # has none, so the core keeps its own. Still an archive (STATIC_LINKING_LINK).
    CFLAGS="-pthread" CXXFLAGS="-pthread" emmake make -C "$src/libretro" platform=emscripten STATIC_LINKING=0 AR=emar -j"$jobs"
    cp "$src/libretro/snes9x_libretro_emscripten.bc" "$out/snes9x_libretro_emscripten.a"
    cp "$src/LICENSE" "$out/snes9x-LICENSE"
fi

echo "web/build-core.sh: $out/${core}_libretro_emscripten.a"
