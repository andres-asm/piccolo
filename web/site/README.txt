The shared look of piccolo's websites (piccolo-site's assets/README.md).

  fetch-site-assets.sh, sync-snippets.sh   copies of piccolo-site's tools/ (keep them the same)
  v1/                                      a committed copy of get-piccolo.app/assets/v1, the fallback

web/package.sh fetches get-piccolo.app/assets/v1 (SITE_ASSETS_URL to change
it) into the web build as _site/v1/, so every build has the look of that
day; when the fetch fails it uses v1/ here. Then it fills the start page's
sites bar and footer from that copy (sync-snippets.sh). Refresh v1/ from
piccolo-site now and then (its MANIFEST checks every file).
