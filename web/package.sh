#!/bin/sh
# Packs the browser build into the folder and zip that deploy by unzipping
# on any static server (docs/wasm.md, web/page/README.txt):
#
#   web/build-core.sh && web/build.sh && web/package.sh
#     -> web/build/dist/piccolo-<version>-web/      (the folder)
#        web/build/dist/piccolo-<version>-web.zip   (the same, zipped)
#
# Layout (all paths relative):
#   index.html                 choose the emulator
#   sameboy/                   piccolo with SameBoy linked in: index.html, piccolo.js/.wasm/.data
#   piccolo-page.js            the page's scripts, with piccolo-storage.js (its folder on OPFS)
#                              and piccolo-files.js (the FILES panel)
#   sw.js                      the service worker (COOP/COEP where the server can't send them,
#                              offline cache)
#   manifest.webmanifest icon-*.png   installable (PWA)
#   .htaccess _headers nginx.conf.example README.txt
#   LICENSES/                  piccolo's and every library's and core's license

set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
build="$here/build"
wasm="$build/wasm"
core=sameboy

version_header="$wasm/generated/piccolo_version.h"
if [ ! -f "$wasm/piccolo.wasm" ] || [ ! -f "$version_header" ]; then
    echo "web/package.sh: no browser build in $wasm (run web/build.sh first)" >&2
    exit 1
fi
version=$(sed -n 's/.*PICCOLO_VERSION "\(.*\)".*/\1/p' "$version_header")
name="piccolo-$version-web"
dist="$build/dist/$name"

rm -rf "$dist" "$build/dist/$name.zip"
mkdir -p "$dist/$core" "$dist/LICENSES"

cp "$here/page/index.html" "$here/page/piccolo-page.js" "$here/page/piccolo-storage.js" "$here/page/piccolo-files.js" "$here/page/sw.js" "$here/page/manifest.webmanifest" \
   "$here/page/.htaccess" "$here/page/_headers" "$here/page/nginx.conf.example" "$here/page/README.txt" "$dist/"
for size in 32 128 512; do
    cp "$root/assets/branding/png/piccolo-icon-$size.png" "$dist/icon-$size.png"
done
cp "$here/page/core.html" "$dist/$core/index.html"
cp "$wasm/piccolo.js" "$wasm/piccolo.wasm" "$wasm/piccolo.data" "$dist/$core/"

# Licenses: piccolo, our libraries, the third-party ones linked in, the core.
cp "$root/LICENSE" "$dist/LICENSES/piccolo.txt"
cp "$root/deps/common/hosts-common/LICENSE" "$dist/LICENSES/hosts-common.txt"
cp "$root/deps/common/frontends-common/LICENSE" "$dist/LICENSES/frontends-common.txt"
cp "$root/deps/common/retroui/LICENSE" "$dist/LICENSES/retroui.txt"
cp "$root/deps/libretro/host-libretro/LICENSE" "$dist/LICENSES/host-libretro.txt"
cp "$root/deps/libretro/frontend-libretro/LICENSE" "$dist/LICENSES/frontend-libretro.txt"
cp "$root/deps/SDL/LICENSE.txt" "$dist/LICENSES/SDL.txt"
cp "$root/deps/imgui/LICENSE.txt" "$dist/LICENSES/imgui.txt"
cp "$root/deps/libsamplerate/COPYING" "$dist/LICENSES/libsamplerate.txt"
cp "$root/deps/miniz/LICENSE" "$dist/LICENSES/miniz.txt"
cp "$root/deps/7zip/DOC/License.txt" "$dist/LICENSES/7zip.txt"
cp "$root/deps/enet/LICENSE" "$dist/LICENSES/enet.txt"
cp "$root/deps/monocypher/LICENCE.md" "$dist/LICENSES/monocypher.txt"
cp "$build/cores/sameboy-LICENSE" "$dist/LICENSES/SameBoy.txt"

(cd "$build/dist" && zip -qr9 "$name.zip" "$name")
echo "web/package.sh: $dist"
echo "web/package.sh: $build/dist/$name.zip ($(du -k "$build/dist/$name.zip" | cut -f1) KB)"
