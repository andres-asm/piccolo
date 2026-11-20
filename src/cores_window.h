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

#include <cstdint>

#include "core_updater.h"
#include "localization.h"

struct ImFont;

struct CoresWindow
{
    bool    open = false;
    int32_t selected = -1;      // index into the updater's index entries
    bool    update_all = false; // UPDATE ALL: install the next outdated core whenever idle
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
