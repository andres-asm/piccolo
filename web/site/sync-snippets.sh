#!/bin/sh
# Puts the shared sites bar and footer into HTML pages, between their markers:
#
#   <!-- sites-header ... -->          <!-- sites-footer ... -->
#   <!-- /sites-header -->             <!-- /sites-footer -->
#
#   tools/sync-snippets.sh SNIPPETS_DIR PAGE...
#
# SNIPPETS_DIR holds sites-header.html and sites-footer.html (assets/v1/ here,
# a synced copy elsewhere). Whatever sits between the markers is replaced;
# the rest of the page is left alone. The other sites' repos carry a copy of
# this script (keep them the same).
set -eu

dir=${1:?usage: sync-snippets.sh SNIPPETS_DIR PAGE...}
shift
for page in "$@"; do
    awk -v header="$dir/sites-header.html" -v footer="$dir/sites-footer.html" '
        function emit(file,   line) { while ((getline line < file) > 0) print line; close(file) }
        /<!-- \/sites-header -->/ { skip = 0 }
        /<!-- \/sites-footer -->/ { skip = 0 }
        !skip { print }
        /<!-- sites-header[ >]/ && !/\/sites-header/ { emit(header); skip = 1 }
        /<!-- sites-footer[ >]/ && !/\/sites-footer/ { emit(footer); skip = 1 }
    ' "$page" > "$page.tmp"
    mv "$page.tmp" "$page"
done
