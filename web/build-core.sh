#!/bin/sh
# Builds a libretro core for the browser, as a static archive that
# web/build.sh links into piccolo.wasm (one wasm per core, docs/wasm.md).
#
#   web/build-core.sh <core>   -> web/build/cores/<core>_libretro_emscripten.a
#                                 web/build/cores/<core>-LICENSE
#
# The recipe is multitap-org/cores' (deps/cores, a submodule): the core's
# pinned source, patches and build, its web platform (Emscripten, -pthread).
# This only runs it and picks up what it writes. Cores with a web build
# there: sameboy, snes9x.
#
# Needs: deps/cores checked out (git submodule update --init deps/cores),
# Emscripten in the environment (see web/build.sh), and what the core's
# recipe needs (SameBoy: cmake, ninja, bison and libpng to build rgbds).

set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
out="$here/build/cores"
cores="$root/deps/cores"
core=${1:-sameboy}

if [ ! -f "$cores/scripts/build-core.sh" ]; then
    echo "web/build-core.sh: no deps/cores (git submodule update --init deps/cores)" >&2
    exit 1
fi
if [ ! -f "$cores/cores/$core/core.cfg" ]; then
    echo "web/build-core.sh: deps/cores has no core '$core'" >&2
    exit 1
fi

# The file name the core's library has (core.cfg file=, e.g. mednafen_pce).
file=$(sed -n 's/^file=//p' "$cores/cores/$core/core.cfg" | head -n 1)

"$cores/scripts/build-core.sh" "$core" web

mkdir -p "$out"
cp "$cores/dist/web/${file}_libretro.a" "$out/${core}_libretro_emscripten.a"
cp "$cores/dist/web/$file-LICENSE" "$out/$core-LICENSE"
echo "web/build-core.sh: $out/${core}_libretro_emscripten.a"
