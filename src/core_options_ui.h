#pragma once

// piccolo's core options UI (decided by Andrés, 2026-11-14): the running
// core's options live in the quick menu (F1); the CORE OPTIONS MANAGER
// window (CONFIG menu) lists and deletes what is stored for any core. Both
// drawn from here. The manager:
//
//   CORE     [ SNES9X (RUNNING)                v ]
//   v STORED VALUES                                       any core, even one that won't start
//     FOR THE CORE                           [DELETE SECTION]
//       CONSOLE REGION = PAL                 [DELETE]
//     [DELETE ALL FOR THIS CORE]
//
// The running core's options are edited live, at the chosen layer (core,
// content folder, game; core_options_store.h), with C / D / G markers like
// the settings' C / G. STORED VALUES lists what core_options.cfg keeps for
// the chosen core, labelled from the definitions cache
// (core_options_cache.h), so a core whose options make it crash on start can
// be put right without loading it. Keys the core no longer declares are
// marked.

#include <string>

#include "core_options_cache.h"
#include "core_options_store.h"
#include "localization.h"

struct CoreSession;
struct ImFont;

struct CoreOptionsUiState
{
    std::string     selected_core;          // "" = the running one (or the first cached)
    CoreOptionLayer layer = CoreOptionLayer::core;
    bool            choose_layer = true;    // pick the most specific layer in use on the next draw
    std::string     section;                // category key shown; "" = the options without a category
};

struct CoreOptionsUiContext
{
    const Localization*     strings = nullptr;
    CoreSession*            session = nullptr;   // live editing while its core runs
    CoreOptionsStore*       store   = nullptr;
    const CoreOptionsCache* cache   = nullptr;
    const char*             running_core = "";   // short name, "" = none
    const char*             folder = "";         // the running content's folder name ("" = none)
    const char*             game = "";           // the running content's name ("" = none)
    float                   label_width = 0.0f;
};

// The manager's contents: CORE (any core the cache or the store knows) and
// its STORED VALUES. Inside a themed window.
void core_options_ui_draw(CoreOptionsUiState* state, const CoreOptionsUiContext* context);

// The CORE OPTIONS MANAGER window. Opening it starts on the running core.
void core_options_manager_open(bool* open, CoreOptionsUiState* state);
void core_options_manager_draw(bool* open, CoreOptionsUiState* state, const CoreOptionsUiContext* context,
                               ImFont* pixel_font, int32_t scale);

// Only the running core's options (SAVE TO and the options), e.g. for the
// quick menu. Nothing when no core runs.
void core_options_ui_draw_running(CoreOptionsUiState* state, const CoreOptionsUiContext* context);
