#pragma once

// Where piccolo's folder lives, before anything reads it.
//
// Desktop and Android (storage.cpp): the real filesystem, nothing to do.
//
// The browser (storage_web.cpp): piccolo's folder (app_paths: settings,
// saves, states, games) lives in the virtual filesystem's memory, and the
// page keeps it on the Origin Private File System (OPFS):
// web/page/piccolo-storage.js copies OPFS into memory before main(), and
// writes what changed back every few seconds, when the tab is hidden, and
// on storage_flush(). Without OPFS piccolo still runs, but nothing is kept
// after the tab closes (the page logs it).

// folder_name: piccolo's folder name (bundled_files_folder_name()).
void storage_init(const char* folder_name);

// After piccolo saved (leaving, or the tab hidden): keep it now, not at the
// next periodic write.
void storage_flush();
