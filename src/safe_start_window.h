#pragma once

// piccolo's safe start (decided by Andrés, 2026-11-14): when the last session
// with a core didn't end properly (a crash or a hang, often right after an
// option change) and that core has stored options, starting it again asks
// first:
//
//   +- SAFE START ---------------------------------------------+
//   | SNES9X DIDN'T CLOSE PROPERLY LAST TIME (A CRASH OR A HANG).|
//   | ITS OPTIONS HAD BEEN CHANGED IN THAT SESSION.              |
//   | IT HAS STORED OPTIONS. START WITH:                         |
//   | [STORED OPTIONS] [CORE DEFAULTS, THIS TIME]                |
//   | [DELETE STORED OPTIONS] [CANCEL]                           |
//   +----------------------------------------------------------+
//
// How a session that didn't end is noticed: piccolo writes
// settings/running.cfg before loading a game and removes it when the game
// ends properly (see main.cpp).

#include <cstdint>

#include "localization.h"

struct ImFont;

enum class SafeStartChoice
{
    none,            // still open
    stored,          // start as usual
    defaults,        // start with the core's own defaults, this time only
    delete_stored,   // delete every stored option of the core, then start
    cancel,          // don't start
};

// Draws the dialog (modal, centered). `core`: the core's short name;
// options_changed: options were changed in the session that didn't end.
SafeStartChoice safe_start_window_draw(ImFont* pixel_font, int32_t scale, const Localization* strings, const char* core,
                                       bool options_changed);
