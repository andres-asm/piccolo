#pragma once

// TOOLS > CORES: the core downloader's window (core_updater.h in
// frontend-libretro does the work).
//
//   CORE         SYSTEMS              VERSION   STATUS
//   SameBoy      Game Boy, Game Bo... v1.0.5    INSTALLED
//   ...                                          (non-free cores marked)
//   ---------------------------------------------------------------
//   LICENSE MIT (FREE)   DOWNLOAD 119 KB   INSTALLED v1.0.5
//   [INSTALL / UPDATE / REPLACE] [REMOVE]        [REFRESH] [UPDATE ALL]
//   status: what the downloader did last, or the download's progress
//
// Opening it gets the list (REFRESH); the list only lives while piccolo
// runs. The running core can't be updated or removed (the downloader
// refuses it and says so).
//
// SHOW NON-FREE CORES (Andrés, 2026-11-20): off by default, so the list has
// only free-licensed cores; turning it on asks for confirmation first,
// explaining what non-free means. Non-free cores already installed are
// always listed (so they can be removed). Store builds never offer them
// (core_updater's free_only).
//
// Core types (Andrés, 2026-11-20): the index's `type` (multitap-org/cores
// chooses it). OLDER VERSIONS (legacy: Snes9x 2005, ...) and EXPERIMENTAL
// are off by default and need no confirmation. Installed cores are always
// listed, whatever the switches say. The three switches are one SHOW row.

#include <cstdint>

#include "core_updater.h"
#include "localization.h"

struct ImFont;

struct CoresWindow
{
    bool    open = false;
    int32_t selected = -1;      // index into the updater's index entries
    bool    update_all = false; // UPDATE ALL: install the next outdated core whenever idle
    float   bottom_height = 0.0f;   // what's under the list, measured last frame (the list takes the rest)
    // The SHOW switches: settings, main.cpp loads them before drawing.
    bool    show_non_free = false;
    bool    show_legacy = false;
    bool    show_experimental = false;
    bool    show_changed = false;            // set when the user changed one; main.cpp saves and clears
    bool    confirm_non_free = false;        // the confirmation is up
};

struct CoresWindowContext
{
    const Localization* strings = nullptr;
    CoreUpdater*        updater = nullptr;
    const char*         running_file = "";   // the running core's file name, "" = none
};

// Opens the window and asks for the list (the updater must be set up first).
void cores_window_open(CoresWindow* window, CoreUpdater* updater);

void cores_window_draw(CoresWindow* window, const CoresWindowContext* context, ImFont* pixel_font, int32_t scale);
