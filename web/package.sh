#!/bin/sh
# Packs the browser build into the folder and zip that deploy by unzipping
# on any static server (docs/wasm.md, web/page/README.txt):
#
#   for core in sameboy snes9x; do web/build-core.sh $core && web/build.sh $core; done
#   web/package.sh
#     -> web/build/dist/piccolo-<version>-web/      (the folder)
#        web/build/dist/piccolo-<version>-web.zip   (the same, zipped)
#
# Layout (all paths relative):
#   index.html                 choose the emulator (it lists every core below)
#   sameboy/ snes9x/           piccolo with that core linked in: index.html, piccolo.js/.wasm/.data
#   piccolo-page.js            the page's scripts, with piccolo-files.js (the FILES panel)
#   multitap-page.js multitap-storage.js   frontends-common's web/: the start, the folder on OPFS
#   sw.js                      the service worker (frontends-common's: COOP/COEP where the server
#                              can't send them, offline cache)
#   manifest.webmanifest icon-*.png   installable (PWA)
#   .htaccess _headers nginx.conf.example README.txt
#   LICENSES/                  piccolo's and every library's and core's license

set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
build="$here/build"
cores="sameboy snes9x"   # index.html links each one

for core in $cores; do
    if [ ! -f "$build/wasm/$core/piccolo.wasm" ]; then
        echo "web/package.sh: no browser build in $build/wasm/$core (run web/build.sh $core first)" >&2
        exit 1
    fi
done
version_header="$build/wasm/sameboy/generated/piccolo_version.h"
version=$(sed -n 's/.*PICCOLO_VERSION "\(.*\)".*/\1/p' "$version_header")
name="piccolo-$version-web"
dist="$build/dist/$name"

rm -rf "$dist" "$build/dist/$name.zip"
mkdir -p "$dist/LICENSES"

cp "$here/page/index.html" "$here/page/piccolo-page.js" "$here/page/piccolo-files.js" "$here/page/manifest.webmanifest" \
   "$here/page/.htaccess" "$here/page/_headers" "$here/page/nginx.conf.example" "$here/page/README.txt" "$dist/"
# The page's shared half (frontends-common's web/: the start, the folder on
# OPFS, the service worker), shared with piccoletto's page.
cp "$root/deps/common/frontends-common/web/multitap-page.js" "$root/deps/common/frontends-common/web/multitap-storage.js" \
   "$root/deps/common/frontends-common/web/sw.js" "$dist/"
for size in 32 128 512; do
    cp "$root/assets/branding/png/piccolo-icon-$size.png" "$dist/icon-$size.png"
done
for core in $cores; do
    mkdir -p "$dist/$core"
    cp "$here/page/core.html" "$dist/$core/index.html"
    cp "$build/wasm/$core/piccolo.js" "$build/wasm/$core/piccolo.wasm" "$build/wasm/$core/piccolo.data" "$dist/$core/"
done

# Licenses: piccolo, our libraries, the third-party ones linked in, the core.
cp "$root/LICENSE" "$dist/LICENSES/piccolo.txt"
cp "$root/deps/common/hosts-common/LICENSE" "$dist/LICENSES/hosts-common.txt"
cp "$root/deps/common/frontends-common/LICENSE" "$dist/LICENSES/frontends-common.txt"
cp "$root/deps/libretro/host-libretro/LICENSE" "$dist/LICENSES/host-libretro.txt"
cp "$root/deps/libretro/frontend-libretro/LICENSE" "$dist/LICENSES/frontend-libretro.txt"
cp "$root/deps/SDL/LICENSE.txt" "$dist/LICENSES/SDL.txt"
cp "$root/deps/imgui/LICENSE.txt" "$dist/LICENSES/imgui.txt"
cp "$root/deps/libsamplerate/COPYING" "$dist/LICENSES/libsamplerate.txt"
cp "$root/deps/miniz/LICENSE" "$dist/LICENSES/miniz.txt"
cp "$root/deps/7zip/DOC/License.txt" "$dist/LICENSES/7zip.txt"
cp "$root/deps/enet/LICENSE" "$dist/LICENSES/enet.txt"
# Nayuki's QR Code generator has its MIT license in the source file's header.
sed -n '1,/\*\//p' "$root/deps/qrcodegen/c/qrcodegen.c" > "$dist/LICENSES/qrcodegen.txt"
cp "$root/deps/monocypher/LICENCE.md" "$dist/LICENSES/monocypher.txt"
cp "$build/cores/sameboy-LICENSE" "$dist/LICENSES/SameBoy.txt"
cp "$build/cores/snes9x-LICENSE" "$dist/LICENSES/Snes9x.txt"

(cd "$build/dist" && zip -qr9 "$name.zip" "$name")
echo "web/package.sh: $dist"
echo "web/package.sh: $build/dist/$name.zip ($(du -k "$build/dist/$name.zip" | cut -f1) KB)"
