#pragma once

// Where piccolo's folder lives, before anything reads it.
//
// Desktop and Android (storage.cpp): the real filesystem, nothing to do.
//
// The browser (storage_web.cpp): piccolo's folder (app_paths: settings,
// saves, states, games) is mounted on the Origin Private File System (OPFS)
// through Emscripten's WasmFS, so it survives reloads; every write reaches
// storage as it happens. Everything else in the virtual filesystem (the
// shipped files, /tmp) lives in memory. Without OPFS piccolo still runs, but
// nothing is kept after the tab closes (logged).

// folder_name: piccolo's folder name (bundled_files_folder_name()).
void storage_init(const char* folder_name);
