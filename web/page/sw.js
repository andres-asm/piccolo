// piccolo's service worker (docs/wasm.md). Two jobs:
//
// 1. Cross-origin isolation where the server can't send the headers
//    (GitHub Pages, a plain static server): every response from this origin
//    gets COOP/COEP, so the page can use SharedArrayBuffer (threads). On a
//    server that already sends them this changes nothing. The same idea as
//    coi-serviceworker (MIT, gzuidhof), written for piccolo.
// 2. Working offline once visited (and as an installed app): responses are
//    kept in a cache; the network comes first, so a new version is picked up
//    as soon as it's online.

'use strict';

const CACHE = 'piccolo-v1';

self.addEventListener('install', function () {
  self.skipWaiting();
});

self.addEventListener('activate', function (event) {
  event.waitUntil(self.clients.claim());
});

function isolated(response) {
  // Opaque responses (cross-origin, no CORS) can't be rewritten; piccolo loads none.
  if (response.status === 0) {
    return response;
  }
  const headers = new Headers(response.headers);
  headers.set('Cross-Origin-Opener-Policy', 'same-origin');
  headers.set('Cross-Origin-Embedder-Policy', 'require-corp');
  headers.set('Cross-Origin-Resource-Policy', 'same-origin');
  return new Response(response.body, { status: response.status, statusText: response.statusText, headers: headers });
}

self.addEventListener('fetch', function (event) {
  const request = event.request;
  if (request.method !== 'GET' || new URL(request.url).origin !== self.location.origin) {
    return;
  }
  // A Chrome quirk: only-if-cached requests outside same-origin mode throw.
  if (request.cache === 'only-if-cached' && request.mode !== 'same-origin') {
    return;
  }
  event.respondWith((async function () {
    try {
      const response = await fetch(request);
      if (response.ok) {
        const cache = await caches.open(CACHE);
        cache.put(request, response.clone());
      }
      return isolated(response);
    } catch (error) {
      const cached = await caches.match(request);
      if (cached) {
        return isolated(cached);
      }
      throw error;
    }
  })());
});
