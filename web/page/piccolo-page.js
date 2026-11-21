// piccolo's page in the browser (docs/wasm.md): what's only piccolo's. The
// start (threads, its folder on OPFS, the module) is frontends-common's
// web/multitap-page.js and multitap-storage.js; the FILES panel is
// piccolo-files.js. This adds opening games (OPEN GAME, drag and drop).

'use strict';

function piccolo_page_files() {
  const button = document.getElementById('open');
  const input = document.getElementById('file');
  button.hidden = false;
  button.addEventListener('click', function () { input.click(); });
  input.addEventListener('change', function () {
    if (input.files.length > 0) {
      multitap_page_open(input.files[0]);
    }
    input.value = '';
  });
  // Drops anywhere on the page. Capture phase, so this runs before SDL's own
  // handler (which would only put the file in /tmp/filedrop).
  let depth = 0;
  window.addEventListener('dragenter', function (event) {
    event.preventDefault();
    depth++;
    document.body.classList.add('dragging');
  }, true);
  window.addEventListener('dragleave', function () {
    depth = Math.max(0, depth - 1);
    if (depth === 0) {
      document.body.classList.remove('dragging');
    }
  }, true);
  window.addEventListener('dragover', function (event) { event.preventDefault(); }, true);
  window.addEventListener('drop', function (event) {
    event.preventDefault();
    event.stopPropagation();
    depth = 0;
    document.body.classList.remove('dragging');
    if (event.dataTransfer.files.length === 0) {
      return;
    }
    // On the FILES panel: into the folder it shows. Elsewhere: play it.
    if (document.getElementById('files').contains(event.target)) {
      piccolo_files_upload(Array.from(event.dataTransfer.files));
    } else {
      multitap_page_open(event.dataTransfer.files[0]);
    }
  }, true);
}

// FULL SCREEN: the game alone on the screen (the stage, not the strip or
// FILES); the browser's Esc leaves it.
function piccolo_page_fullscreen() {
  const button = document.getElementById('fullscreen');
  const stage = document.getElementById('stage');
  if (!stage.requestFullscreen) {
    button.hidden = true;
    return;
  }
  button.addEventListener('click', function () {
    if (document.fullscreenElement) {
      document.exitFullscreen();
    } else {
      stage.requestFullscreen().catch(function (error) { console.log('Full screen: ' + error.message); });
    }
    document.getElementById('canvas').focus();
  });
  document.addEventListener('fullscreenchange', function () {
    button.classList.toggle('active', document.fullscreenElement === stage);
  });
}

// The page as before, kept for scripts that call it (tests).
function piccolo_page_open(file) {
  return multitap_page_open(file);
}

function piccolo_page_start() {
  // The dev channel (every push to main) says so, with its commit; a release says nothing.
  if (document.body.dataset.channel === 'dev') {
    document.getElementById('channel').hidden = false;
  }
  piccolo_page_fullscreen();
  multitap_page_start({
    name: 'piccolo',
    title: 'PICCOLO',
    // piccolo's folder is OPFS's root (where its first browser builds kept
    // it); piccoletto's folder there is piccoletto's. cache/: extracted
    // archives, made again when needed.
    opfs_folder: '',
    skipped: ['cache', 'piccoletto'],
    sw: '../sw.js',
    sw_scope: '../',
    started: function () {
      piccolo_page_files();
      piccolo_files_start();
    },
  });
}
