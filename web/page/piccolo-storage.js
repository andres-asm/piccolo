// piccolo's folder in the browser (docs/wasm.md): kept on OPFS (the origin
// private file system), used from memory.
//
// piccolo reads and writes its folder (settings, saves, states, games) in the
// virtual filesystem's memory, which never blocks. This file keeps it on
// OPFS with OPFS's own async API, from the page:
//
// - before piccolo's main(), everything on OPFS is copied into memory;
// - piccolo_storage_sync() writes what changed back (and removes what's gone):
//   every few seconds, when the tab is hidden, and after piccolo saves.
//
// Why not Emscripten's OPFS backend (WasmFS) mounted as the folder: it works
// in Chrome, but in Firefox renames fail (settings are written to a temporary
// file and renamed) and reading a file written earlier blocks forever, so the
// page hangs at start. The async API works the same in every browser.

'use strict';

// piccolo's folder: XDG_DATA_HOME (set below before main) + "/piccolo".
const PICCOLO_DATA = '/data';
const PICCOLO_FOLDER = PICCOLO_DATA + '/piccolo';
const PICCOLO_SYNC_SECONDS = 5;

const piccolo_storage = {
  root: null,          // OPFS's root folder (FileSystemDirectoryHandle), null without OPFS
  loaded: [],          // [path inside the folder, bytes]: read before the module starts
  known: new Map(),    // path inside the folder -> "size:mtime" (or "dir") as last written to OPFS
  busy: false,
  again: false,
};

// Rebuilt from what's kept (cache/: extracted archives, made again when needed).
function piccolo_storage_skipped(relative) {
  return relative === 'cache' || relative.startsWith('cache/');
}

function piccolo_storage_join(parent, name) {
  return parent === '' ? name : parent + '/' + name;
}

async function piccolo_storage_read_folder(handle, relative) {
  for await (const [name, entry] of handle.entries()) {
    const path = piccolo_storage_join(relative, name);
    if (piccolo_storage_skipped(path)) {
      continue;
    }
    if (entry.kind === 'directory') {
      piccolo_storage.loaded.push([path, null]);
      await piccolo_storage_read_folder(entry, path);
    } else {
      const file = await entry.getFile();
      piccolo_storage.loaded.push([path, new Uint8Array(await file.arrayBuffer())]);
    }
  }
}

// Step 1, before piccolo.js loads: OPFS -> a list in JS memory.
async function piccolo_storage_read() {
  try {
    piccolo_storage.root = await navigator.storage.getDirectory();
  } catch (error) {
    console.log('Storage: no OPFS here (' + error.message + '): nothing is kept after this tab closes');
    piccolo_storage.root = null;
    return;
  }
  try {
    await piccolo_storage_read_folder(piccolo_storage.root, '');
  } catch (error) {
    console.log('Storage: can\'t read OPFS: ' + error.message);
  }
}

// stat.mode's file type (WasmFS's FS has no isDir).
function piccolo_storage_is_folder(mode) {
  return (mode & 0o170000) === 0o040000;
}

function piccolo_storage_key(stat) {
  return stat.size + ':' + Number(stat.mtime);
}

// Step 2, from Module.preRun: piccolo's folder is XDG_DATA_HOME/piccolo.
function piccolo_storage_environment() {
  Module.ENV.XDG_DATA_HOME = PICCOLO_DATA;
}

// Step 3, from Module.onRuntimeInitialized (the filesystem exists, main()
// hasn't run): the list -> piccolo's folder in memory.
function piccolo_storage_install() {
  Module.FS.mkdirTree(PICCOLO_FOLDER);
  for (const [path, data] of piccolo_storage.loaded) {
    const full = PICCOLO_FOLDER + '/' + path;
    try {
      if (data === null) {
        Module.FS.mkdirTree(full);
        piccolo_storage.known.set(path, 'dir');
      } else {
        Module.FS.writeFile(full, data);
        piccolo_storage.known.set(path, piccolo_storage_key(Module.FS.stat(full)));
      }
    } catch (error) {
      console.log('Storage: can\'t restore ' + path + ': ' + error.message);
    }
  }
  console.log('Storage: ' + piccolo_storage.loaded.length + ' entries from OPFS into ' + PICCOLO_FOLDER);
  piccolo_storage.loaded = [];
  if (piccolo_storage.root !== null) {
    setInterval(piccolo_storage_sync, PICCOLO_SYNC_SECONDS * 1000);
    document.addEventListener('visibilitychange', function () {
      if (document.hidden) {
        piccolo_storage_sync();
      }
    });
  }
}

async function piccolo_storage_write_folder(handle, relative, seen) {
  const folder = relative === '' ? PICCOLO_FOLDER : PICCOLO_FOLDER + '/' + relative;
  for (const name of Module.FS.readdir(folder)) {
    if (name === '.' || name === '..') {
      continue;
    }
    const path = piccolo_storage_join(relative, name);
    if (piccolo_storage_skipped(path)) {
      continue;
    }
    const full = folder + '/' + name;
    let stat;
    try {
      stat = Module.FS.stat(full);
    } catch (error) {
      continue;   // removed since readdir
    }
    seen.add(path);
    if (piccolo_storage_is_folder(stat.mode)) {
      const child = await handle.getDirectoryHandle(name, { create: true });
      piccolo_storage.known.set(path, 'dir');
      await piccolo_storage_write_folder(child, path, seen);
      continue;
    }
    const key = piccolo_storage_key(stat);
    if (piccolo_storage.known.get(path) === key) {
      continue;
    }
    // A copy: the module's memory is shared (threads), and OPFS takes no
    // views of shared memory.
    const data = new Uint8Array(Module.FS.readFile(full));
    const file = await handle.getFileHandle(name, { create: true });
    const writable = await file.createWritable();
    await writable.write(data);
    await writable.close();
    piccolo_storage.known.set(path, key);
  }
}

async function piccolo_storage_remove(path) {
  const parts = path.split('/');
  const name = parts.pop();
  let handle = piccolo_storage.root;
  for (const part of parts) {
    handle = await handle.getDirectoryHandle(part);
  }
  await handle.removeEntry(name, { recursive: true });
}

async function piccolo_storage_sync_once() {
  const seen = new Set();
  await piccolo_storage_write_folder(piccolo_storage.root, '', seen);
  // Gone from memory: gone from OPFS. Parents first is fine (recursive).
  for (const path of Array.from(piccolo_storage.known.keys())) {
    if (!seen.has(path)) {
      piccolo_storage.known.delete(path);
      try {
        await piccolo_storage_remove(path);
      } catch (error) {
        // Already gone with its folder.
      }
    }
  }
}

// Memory -> OPFS, what changed since the last time. One at a time: a call
// while one runs makes it go once more when it ends.
async function piccolo_storage_sync() {
  if (piccolo_storage.root === null || typeof Module === 'undefined' || !Module.FS) {
    return;
  }
  if (piccolo_storage.busy) {
    piccolo_storage.again = true;
    return;
  }
  piccolo_storage.busy = true;
  try {
    do {
      piccolo_storage.again = false;
      await piccolo_storage_sync_once();
    } while (piccolo_storage.again);
  } catch (error) {
    console.log('Storage: can\'t write to OPFS: ' + error.message);
  } finally {
    piccolo_storage.busy = false;
  }
}
