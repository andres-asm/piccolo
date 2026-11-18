#pragma once

// SETTINGS > INPUT (milestone 3, step 4), one level of tabs:
//
//   SETTINGS   the input settings, with the SCOPE selector (GLOBAL / THIS
//              CORE / THIS GAME): keyboard shares port 1. (HOTKEYS has the
//              hotkey mode the same way, above its bindings.)
//   RETROPAD   the bindings of one controller (MAPPING: the one on port 1
//   INTERFACE  unless another is picked), no scope (they're the
//   HOTKEYS    controller's): GAME / GAMEPAD (or KEY) columns, SET / CLEAR,
//              RESET on changed rows (shown highlighted), BIND ALL asks for
//              each in turn (5 s each skips one, Esc stops).
//   DEVICES    every device: state, NAME, DEADZONE, IGNORE; keyboard players.
//
// Nothing about the running game here: a core's own devices are asked for
// in the game (step 5). Binding changes mark the window dirty (piccolo
// saves settings/input.cfg); the settings go to piccolo.cfg.

#include <cstdint>
#include <string>
#include <vector>

#include "input_bindings.h"
#include "input_layouts.h"
#include "input_sdl.h"
#include "localization.h"
#include "settings.h"

struct ImFont;

struct InputWindow
{
    bool         open = false;
    int          tab = 0;                    // 0 SETTINGS .. 3 DEVICES (for PAGE LEFT / RIGHT)
    SettingLayer scope = SettingLayer::global;
    bool         choose_scope = false;       // pick the scope on the next draw (just opened)
    uint32_t     mapping = UINT32_MAX;       // the controller mapped in RETROPAD / INTERFACE / HOTKEYS (UINT32_MAX: port 1's)

    bool        capturing = false;           // SET: waiting for a key or button
    InputAction capture_action = InputAction::pad_b;
    uint32_t    capture_row = 0;             // the row (source) being set; its count = a new row (+)
    uint32_t    capture_controller = 0;
    InputBindingSet capture_backup;          // the controller's bindings as the SET / BIND ALL began (CANCEL)
    InputAction pad_action = InputAction::count;   // the row the floating pad shows (focused or hovered)
    uint32_t    pad_row = 0;
    uint64_t    capture_started_ms = 0;
    bool        bind_all = false;            // BIND ALL: go on to the next action...
    InputAction bind_all_last = InputAction::pad_b;   // ...up to this one

    std::vector<std::string> names;          // NAME fields being edited, per controller
    uint32_t     map_request = 0;            // DEVICES > MAP...: this joystick (piccolo opens MAP CONTROLLER)
    bool dirty = false;                      // bindings or devices changed: save settings/input.cfg
};

struct InputWindowContext
{
    InputSystem*        input = nullptr;
    const InputSdl*     sdl = nullptr;
    const Localization* strings = nullptr;
    Settings*           settings = nullptr;
    const char*         core_name = "";      // "" = no core running (SCOPE)
    const char*         game_name = "";
    const InputLayouts* layouts = nullptr;   // the controllers' button names
    uint64_t            now_ms = 0;
};

void input_window_open(InputWindow* window);

// Draws the window while open (and runs a SET / BIND ALL in progress).
void input_window_draw(InputWindow* window, const InputWindowContext* context, ImFont* pixel_font, int32_t scale);

// A gamepad's layout (its appearance, as kept in its profile), or nullptr
// when it has none (never mapped, declined, or a layout that's gone).
const InputLayout* input_window_layout(const InputSystem* input, const InputLayouts* layouts, const InputDevice* device);

// A source as the user sees it on that device: "RETURN", "A", "CROSS",
// "D-PAD UP", "LEFT STICK RIGHT", "ZL"... A button the controller's profile
// gave a name to has that name; the rest are named by its layout, without
// one by SDL's labels. `profile` may be nullptr.
void input_window_source_label(const InputSdl* sdl, const InputDevice* device, const InputLayout* layout,
                               const InputProfile* profile, InputSource source, char* out, size_t out_size);

// A binding's sources in `width`: all of them ("D-PAD UP, LEFT STICK UP"),
// or when they don't fit the first and how many more ("D-PAD UP +1") with
// all of them in the tooltip; one alone that doesn't fit is cut short with
// "…" (all of it in the tooltip); NONE when there's none.
void input_window_draw_sources(const InputSdl* sdl, const InputDevice* device, const InputLayout* layout,
                               const InputProfile* profile, const InputSource* sources, float width, const Localization* strings);

// Waiting for a binding: PRESS A BUTTON (4) / PRESS A KEY (4).
void input_window_draw_waiting(bool keyboard, uint64_t seconds_left, float hold_progress, const Localization* strings);

// The defaults a controller goes back to with RESET / DEFAULTS: the
// keyboard's or a gamepad's, none for the keyboard's extra players.
void input_window_controller_defaults(const InputSystem* input, uint32_t controller, InputBindingSet* set);
