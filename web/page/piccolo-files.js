// The FILES panel next to piccolo (docs/wasm.md): the session's filesystem,
// what piccolo sees, starting in its folder (saves, states, settings, games).
// Download, upload, delete, and open a file in piccolo. Changes inside
// piccolo's folder reach OPFS with the next storage sync (piccolo-storage.js).

'use strict';

const piccolo_files = {
  folder: '',          // the folder shown
  signature: '',       // what was drawn last (redrawn only when it changes)
  timer: 0,
};

const PICCOLO_FILES_OPEN_KEY = 'piccolo-files-open';

function piccolo_files_element(id) {
  return document.getElementById(id);
}

function piccolo_files_size(bytes) {
  if (bytes < 1024) {
    return bytes + ' B';
  }
  if (bytes < 1024 * 1024) {
    return (bytes / 1024).toFixed(1) + ' KB';
  }
  return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
}

function piccolo_files_join(folder, name) {
  return folder === '/' ? '/' + name : folder + '/' + name;
}

function piccolo_files_parent(folder) {
  const slash = folder.lastIndexOf('/');
  return slash <= 0 ? '/' : folder.substring(0, slash);
}

// The folder's entries: folders first, then files, each by name.
function piccolo_files_list(folder) {
  const entries = [];
  for (const name of Module.FS.readdir(folder)) {
    if (name === '.' || name === '..') {
      continue;
    }
    try {
      const stat = Module.FS.stat(piccolo_files_join(folder, name));
      entries.push({ name: name, folder: piccolo_storage_is_folder(stat.mode), size: stat.size, mtime: Number(stat.mtime) });
    } catch (error) {
      // Gone since readdir.
    }
  }
  entries.sort(function (a, b) {
    if (a.folder !== b.folder) {
      return a.folder ? -1 : 1;
    }
    return a.name.localeCompare(b.name);
  });
  return entries;
}

function piccolo_files_button(text, title, action) {
  const button = document.createElement('button');
  button.textContent = text;
  button.title = title;
  button.addEventListener('click', function (event) {
    event.stopPropagation();
    action();
  });
  return button;
}

function piccolo_files_download(path) {
  // A copy: a Blob can't hold a view of the module's shared memory.
  const data = new Uint8Array(Module.FS.readFile(path));
  const url = URL.createObjectURL(new Blob([data]));
  const link = document.createElement('a');
  link.href = url;
  link.download = path.substring(path.lastIndexOf('/') + 1);
  document.body.appendChild(link);
  link.click();
  link.remove();
  setTimeout(function () { URL.revokeObjectURL(url); }, 10000);
}

function piccolo_files_remove_tree(path) {
  if (piccolo_storage_is_folder(Module.FS.stat(path).mode)) {
    for (const name of Module.FS.readdir(path)) {
      if (name !== '.' && name !== '..') {
        piccolo_files_remove_tree(path + '/' + name);
      }
    }
    Module.FS.rmdir(path);
  } else {
    Module.FS.unlink(path);
  }
}

function piccolo_files_delete(path, folder) {
  const what = path.substring(path.lastIndexOf('/') + 1) + (folder ? ' AND EVERYTHING IN IT' : '');
  if (!window.confirm('DELETE ' + what + '?')) {
    return;
  }
  try {
    piccolo_files_remove_tree(path);
  } catch (error) {
    window.alert('CAN\'T DELETE: ' + error.message);
  }
  piccolo_storage_sync();
  piccolo_files_refresh(true);
}

function piccolo_files_open_in_piccolo(path) {
  Module.ccall('piccolo_web_open', null, ['string'], [path]);
  piccolo_files_element('canvas').focus();
}

async function piccolo_files_upload(files) {
  for (const file of files) {
    const data = new Uint8Array(await file.arrayBuffer());
    const name = file.name.replace(/[\/\\]/g, '_');
    try {
      Module.FS.writeFile(piccolo_files_join(piccolo_files.folder, name), data);
    } catch (error) {
      window.alert('CAN\'T WRITE ' + name + ': ' + error.message);
    }
  }
  piccolo_storage_sync();
  piccolo_files_refresh(true);
}

function piccolo_files_go(folder) {
  piccolo_files.folder = folder;
  piccolo_files_refresh(true);
}

function piccolo_files_draw_path() {
  const path = piccolo_files_element('files-path');
  path.textContent = '';
  const parts = piccolo_files.folder.split('/').filter(function (part) { return part !== ''; });
  let target = '';
  const root = piccolo_files_button('/', 'The top of the filesystem', function () { piccolo_files_go('/'); });
  path.appendChild(root);
  for (const part of parts) {
    target += '/' + part;
    const where = target;
    path.appendChild(piccolo_files_button(part + '/', where, function () { piccolo_files_go(where); }));
  }
}

function piccolo_files_draw(entries) {
  piccolo_files_draw_path();
  const list = piccolo_files_element('files-list');
  list.textContent = '';
  if (piccolo_files.folder !== '/') {
    const up = document.createElement('li');
    up.className = 'folder';
    up.innerHTML = '<span class="name">..</span>';
    up.addEventListener('click', function () { piccolo_files_go(piccolo_files_parent(piccolo_files.folder)); });
    list.appendChild(up);
  }
  for (const entry of entries) {
    const path = piccolo_files_join(piccolo_files.folder, entry.name);
    const row = document.createElement('li');
    row.className = entry.folder ? 'folder' : 'file';
    const name = document.createElement('span');
    name.className = 'name';
    name.textContent = entry.folder ? entry.name + '/' : entry.name;
    name.title = path;
    row.appendChild(name);
    const size = document.createElement('span');
    size.className = 'size';
    size.textContent = entry.folder ? '' : piccolo_files_size(entry.size);
    row.appendChild(size);
    const actions = document.createElement('span');
    actions.className = 'actions';
    if (entry.folder) {
      row.addEventListener('click', function () { piccolo_files_go(path); });
    } else {
      actions.appendChild(piccolo_files_button('PLAY', 'Open in piccolo', function () { piccolo_files_open_in_piccolo(path); }));
      actions.appendChild(piccolo_files_button('GET', 'Download', function () { piccolo_files_download(path); }));
      row.addEventListener('dblclick', function () { piccolo_files_open_in_piccolo(path); });
    }
    actions.appendChild(piccolo_files_button('DEL', 'Delete', function () { piccolo_files_delete(path, entry.folder); }));
    row.appendChild(actions);
    list.appendChild(row);
  }
  if (entries.length === 0) {
    const empty = document.createElement('li');
    empty.className = 'empty';
    empty.textContent = 'EMPTY';
    list.appendChild(empty);
  }
}

async function piccolo_files_draw_usage() {
  const usage = piccolo_files_element('files-usage');
  if (piccolo_storage.root === null) {
    usage.textContent = 'NOT KEPT: THIS BROWSER HAS NO OPFS';
    return;
  }
  let text = 'KEPT IN THIS BROWSER (OPFS)';
  if (navigator.storage && navigator.storage.estimate) {
    try {
      const estimate = await navigator.storage.estimate();
      text += ' · ' + piccolo_files_size(estimate.usage || 0) + ' USED';
    } catch (error) {
      // No estimate: the line without it.
    }
  }
  usage.textContent = text;
}

// Redraws when what's in the folder changed (or always, after an action).
function piccolo_files_refresh(always) {
  if (piccolo_files_element('files').classList.contains('closed')) {
    return;
  }
  let entries;
  try {
    entries = piccolo_files_list(piccolo_files.folder);
  } catch (error) {
    // The folder is gone: back to piccolo's.
    piccolo_files.folder = PICCOLO_FOLDER;
    entries = piccolo_files_list(piccolo_files.folder);
  }
  const signature = piccolo_files.folder + '|' + JSON.stringify(entries);
  if (!always && signature === piccolo_files.signature) {
    return;
  }
  piccolo_files.signature = signature;
  piccolo_files_draw(entries);
  piccolo_files_draw_usage();
}

function piccolo_files_set_open(open) {
  piccolo_files_element('files').classList.toggle('closed', !open);
  piccolo_files_element('files-toggle').classList.toggle('active', open);
  try {
    localStorage.setItem(PICCOLO_FILES_OPEN_KEY, open ? '1' : '0');
  } catch (error) {
    // Not remembered: fine.
  }
  if (open) {
    piccolo_files_refresh(true);
  }
}

// From Module.onRuntimeInitialized (the filesystem exists).
function piccolo_files_start() {
  piccolo_files.folder = PICCOLO_FOLDER;
  const toggle = piccolo_files_element('files-toggle');
  toggle.hidden = false;
  toggle.addEventListener('click', function () {
    piccolo_files_set_open(piccolo_files_element('files').classList.contains('closed'));
  });
  piccolo_files_element('files-close').addEventListener('click', function () { piccolo_files_set_open(false); });
  piccolo_files_element('files-home').addEventListener('click', function () { piccolo_files_go(PICCOLO_FOLDER); });
  piccolo_files_element('files-refresh').addEventListener('click', function () { piccolo_files_refresh(true); });
  const input = piccolo_files_element('files-input');
  piccolo_files_element('files-upload').addEventListener('click', function () { input.click(); });
  input.addEventListener('change', function () {
    if (input.files.length > 0) {
      piccolo_files_upload(Array.from(input.files));
    }
    input.value = '';
  });
  let open = false;
  try {
    open = localStorage.getItem(PICCOLO_FILES_OPEN_KEY) === '1';
  } catch (error) {
    open = false;
  }
  piccolo_files_set_open(open);
  // piccolo writes files as it plays (saves, states): keep the list current.
  piccolo_files.timer = setInterval(function () { piccolo_files_refresh(false); }, 2000);
}
