#pragma once

// Applies the Display settings to piccolo's window: fullscreen (borderless /
// exclusive at the desktop resolution), window mode (fixed size / dynamic,
// window_size.h) and the remembered size and position.
//
// Called every frame; it only touches the window when what it should be
// changes (a setting, the game's picture size, the mode), so the user can
// still resize the window by hand in between.

#include <cstdint>

#include "settings.h"
#include "viewport.h"

struct SDL_Window;

struct PiccoloWindow
{
    // What was applied last (to notice changes).
    bool     applied            = false;
    bool     fullscreen         = false;
    uint32_t fullscreen_type    = 0;
    uint32_t mode               = 0;
    double   fixed_aspect       = 0.0;
    uint32_t fixed_height       = 0;
    uint32_t dynamic_width      = 0;   // the size dynamic mode asked for
    uint32_t dynamic_height     = 0;

    // For the settings window: the size the current mode gives (pixels).
    uint32_t target_width  = 0;
    uint32_t target_height = 0;
    uint32_t target_scale  = 0;   // dynamic: the factor that fit

    // Remembered windowed size and position (window coordinates), from and
    // for the "last used" file.
    bool    has_saved   = false;
    int32_t saved_x     = 0;
    int32_t saved_y     = 0;
    int32_t saved_width = 0;
    int32_t saved_height = 0;
};

// At startup, after the window exists and the settings are loaded: the
// remembered size and position, if that setting is on.
void piccolo_window_restore(PiccoloWindow* state, SDL_Window* window, const Settings* settings);

// Every frame. `picture` = the running game's frame and Picture settings
// (window fields unused), or nullptr when no game runs.
void piccolo_window_apply(PiccoloWindow* state, SDL_Window* window, const Settings* settings,
                          const ViewportInput* picture);

// Before saving the "last used" file: the window's current windowed size and
// position (skipped while fullscreen, so the windowed ones are kept).
void piccolo_window_capture(PiccoloWindow* state, SDL_Window* window);
