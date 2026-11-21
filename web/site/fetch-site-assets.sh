#!/bin/sh
# Copies the shared look (assets/v1/ of get-piccolo.app) into a folder, the
# way the other sites keep their own copy (assets/v1/README.md):
#
#   fetch-site-assets.sh DEST            from https://get-piccolo.app/assets/v1
#   SITE_ASSETS_URL=<url> fetch-site-assets.sh DEST
#
# Reads MANIFEST, downloads every file it lists, checks each SHA-256, then
# swaps the new folder in whole (DEST is never half updated; a failed fetch
# leaves the old copy as it was). Exit 0: DEST changed; 1: failed;
# 2: DEST already matched or another run is updating it. Needs curl and sha256sum. The other sites' repos
# carry a copy of this script (keep them the same).
set -u

dest=${1:?usage: fetch-site-assets.sh DEST}
url=${SITE_ASSETS_URL:-https://get-piccolo.app/assets/v1}
dest=${dest%/}
parent=$(dirname "$dest")
mkdir -p "$parent" || exit 1
# One update at a time (two publishers can share a site): a lock folder,
# taken over when it's older than 10 minutes (a killed run).
lock="$dest.lock"
if ! mkdir "$lock" 2>/dev/null; then
    if [ -n "$(find "$lock" -maxdepth 0 -mmin +10 2>/dev/null)" ]; then
        rm -rf "$lock" && mkdir "$lock" 2>/dev/null || exit 2
    else
        exit 2
    fi
fi
work=$(mktemp -d "$parent/.site-assets.XXXXXX") || { rmdir "$lock"; exit 1; }
trap 'rm -rf "$work"; rmdir "$lock"' EXIT

curl -fsSL --max-time 60 -o "$work/MANIFEST" "$url/MANIFEST" || { echo "fetch-site-assets: no $url/MANIFEST" >&2; exit 1; }
if [ -f "$dest/MANIFEST" ] && cmp -s "$work/MANIFEST" "$dest/MANIFEST"; then
    exit 2
fi
# Paths: letters, digits, - _ . / only, never "..", so nothing lands outside $work.
if grep -vqE '^[0-9a-f]{64}  [A-Za-z0-9_][A-Za-z0-9_./-]*$' "$work/MANIFEST" || grep -q '\.\.' "$work/MANIFEST"; then
    echo "fetch-site-assets: unexpected line in $url/MANIFEST" >&2
    exit 1
fi
while read -r sum path; do
    mkdir -p "$work/$(dirname "$path")" &&
        curl -fsSL --max-time 60 -o "$work/$path" "$url/$path" || { echo "fetch-site-assets: no $url/$path" >&2; exit 1; }
done < "$work/MANIFEST"
(cd "$work" && sha256sum -c --quiet MANIFEST) || { echo "fetch-site-assets: checksum mismatch" >&2; exit 1; }
chmod -R a+rX "$work"
# Swap: the old copy aside, the new one in, the old one gone.
old=""
if [ -e "$dest" ]; then
    old="$work.old"
    mv "$dest" "$old" || exit 1
fi
mv "$work" "$dest" || { [ -n "$old" ] && mv "$old" "$dest"; exit 1; }
[ -n "$old" ] && rm -rf "$old"
exit 0
