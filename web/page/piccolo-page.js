// piccolo's page in the browser (docs/wasm.md): starts the module, checks
// that threads are possible, opens files (button, drag and drop). Its folder
// on OPFS: piccolo-storage.js; the FILES panel: piccolo-files.js.
//
// Threads need the page to be "cross-origin isolated" (COOP/COEP headers).
// A server that sends them (the Docker image, the .htaccess / nginx /
// _headers files in this folder) needs nothing else. Elsewhere (GitHub Pages,
// python -m http.server) sw.js adds them: the first visit registers it and
// reloads once.

'use strict';

function piccolo_page_message(text, show_progress) {
  document.getElementById('message').textContent = text;
  document.getElementById('progress').style.display = show_progress ? '' : 'none';
  document.getElementById('overlay').classList.remove('gone');
}

// Threads: crossOriginIsolated, or sw.js to make it so (one reload).
// Resolves true when the module can start; false when it can't (a message is up).
async function piccolo_page_isolation() {
  if (self.crossOriginIsolated) {
    return true;
  }
  if (!window.isSecureContext || !('serviceWorker' in navigator)) {
    piccolo_page_message('THIS PAGE NEEDS HTTPS (OR LOCALHOST) AND SERVICE WORKERS, OR A SERVER THAT SENDS ' +
                         'THE COOP/COEP HEADERS (SEE README.TXT)', false);
    return false;
  }
  // One reload only: if it didn't help, say so instead of looping.
  if (sessionStorage.getItem('piccolo-sw-reloaded')) {
    sessionStorage.removeItem('piccolo-sw-reloaded');
    piccolo_page_message('THIS BROWSER BLOCKS WHAT PICCOLO NEEDS (SERVICE WORKERS, OR SHAREDARRAYBUFFER). ' +
                         'A PRIVATE WINDOW CAN DO THAT: TRY A NORMAL ONE.', false);
    return false;
  }
  try {
    await navigator.serviceWorker.register('../sw.js', { scope: '../' });
    await navigator.serviceWorker.ready;
    sessionStorage.setItem('piccolo-sw-reloaded', '1');
    location.reload();
  } catch (error) {
    piccolo_page_message('SERVICE WORKER REFUSED: ' + error.message, false);
  }
  return false;
}

// A file the person chose or dropped: into the virtual filesystem, then piccolo
// keeps it in its games folder (on OPFS) and starts it (piccolo_web_open).
async function piccolo_page_open(file) {
  const data = new Uint8Array(await file.arrayBuffer());
  const folder = '/tmp/open';
  try { Module.FS.mkdir(folder); } catch (error) { /* already there */ }
  const name = file.name.replace(/[\/\\]/g, '_');
  Module.FS.writeFile(folder + '/' + name, data);
  Module.ccall('piccolo_web_open', null, ['string'], [folder + '/' + name]);
  document.getElementById('canvas').focus();
}

function piccolo_page_files() {
  const button = document.getElementById('open');
  const input = document.getElementById('file');
  button.hidden = false;
  button.addEventListener('click', function () { input.click(); });
  input.addEventListener('change', function () {
    if (input.files.length > 0) {
      piccolo_page_open(input.files[0]);
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
      piccolo_page_open(event.dataTransfer.files[0]);
    }
  }, true);
}

// SDL takes the window's size from the canvas's CSS size, but only on the
// browser's resize event: send one whenever the stage changes (the FILES
// panel opening or closing, the page resized).
function piccolo_page_follow_stage() {
  let pending = false;
  const observer = new ResizeObserver(function () {
    if (pending) {
      return;
    }
    pending = true;
    requestAnimationFrame(function () {
      pending = false;
      window.dispatchEvent(new Event('resize'));
    });
  });
  observer.observe(document.getElementById('stage'));
}

async function piccolo_page_start() {
  if (!(await piccolo_page_isolation())) {
    return;
  }
  if (!document.createElement('canvas').getContext('webgl2')) {
    piccolo_page_message('THIS BROWSER HAS NO WEBGL 2', false);
    return;
  }
  // Ask the browser to keep piccolo's folder (saves, states) when space runs
  // low; it may say no, and then it's only kept while space allows.
  if (navigator.storage && navigator.storage.persist) {
    navigator.storage.persist();
  }
  // piccolo's folder from OPFS, before the module starts (piccolo-storage.js).
  piccolo_page_message('LOADING PICCOLO', true);
  await piccolo_storage_read();
  window.Module = {
    canvas: document.getElementById('canvas'),
    print: function (text) { console.log(text); },
    printErr: function (text) { console.log(text); },
    setStatus: function (text) {
      // Emscripten's download progress: "Downloading data... (12/34)".
      const match = /\((\d+(?:\.\d+)?)\/(\d+)\)/.exec(text);
      if (match) {
        const progress = document.getElementById('progress');
        progress.max = Number(match[2]);
        progress.value = Number(match[1]);
      }
    },
    preRun: [piccolo_storage_environment],
    // The filesystem exists and main() hasn't run: piccolo's folder goes in now.
    onRuntimeInitialized: function () {
      piccolo_storage_install();
      document.getElementById('overlay').classList.add('gone');
      piccolo_page_files();
      piccolo_files_start();
      piccolo_page_follow_stage();
      document.getElementById('canvas').focus();
    },
    onAbort: function (what) {
      piccolo_page_message('PICCOLO STOPPED: ' + what, false);
    },
  };
  const script = document.createElement('script');
  script.src = 'piccolo.js';
  script.onerror = function () { piccolo_page_message('CAN\'T LOAD PICCOLO.JS', false); };
  document.body.appendChild(script);
}
