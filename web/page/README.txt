piccolo in the browser
======================

Unzip this folder anywhere on a web server and open its index.html through
the server (http:// or https://). Any path works, a subfolder too.

piccolo runs threads, which browsers only allow on a "cross-origin isolated"
page. Three ways to get that:

  1. The server sends the headers (best: no extra reload).
       Apache: the .htaccess here (mod_headers on).
       nginx: nginx.conf.example.
       Cloudflare Pages / Netlify: the _headers file here.
       Docker: the piccolo-web image serves this folder with them.
  2. Nothing to configure (GitHub Pages, python3 -m http.server): sw.js, a
     service worker, adds the headers. The first visit reloads once.
     Needs https:// (or localhost) and a browser that allows service workers
     (some private windows don't).
  3. Opening index.html straight from disk (file://) does NOT work.

Browsers: recent Chrome, Edge, Firefox, Safari, with WebGL 2.

Saves, savestates and settings are kept in the browser (the Origin Private
File System) for this site's address. No games are included: open your own.
The FILES button shows them: download, add, delete, or play a file.

Licenses: LICENSES/ (piccolo is MIT; each core and library under its own).
