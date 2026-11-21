#pragma once

// The on-screen gamepad (Andrés, 2026-11-20): piccoletto's touch pad
// (retroui's pad_layout, the same layouts and EDIT LAYOUT) over the game.
//
//   - A controller like any other in the input router: "TOUCH PAD", a
//     device that hands a RetroPad (input_devices_set_retropad, as phones'
//     pads do). START joins a port; remaps, turbo, DEVICES all work on it.
//   - Landscape: over the picture (see-through, SETTINGS > INPUT > OPACITY).
//     Upright: under the picture when it fits (the picture is drawn in the
//     space above it), else over its lower part (a tall picture).
//   - The middle button is MENU (retroui's action button): the QUICK MENU.
//   - SHOW: AUTO (a touch screen: shown until a real gamepad plays, a tap
//     brings it back), ON, OFF.
//   - EDIT LAYOUT (QUICK MENU > CONTROLS): piccoletto's editor; saved in
//     <settings>/touch_layouts.cfg.
//
// Fingers come from SDL's finger events (touch_fingers); with the mouse on
// a desktop (left button held), so it can be tried there too.

#include <cstdint>
#include <string>

#include "haptics.h"
#include "input_bindings.h"
#include "pad_editor.h"
#include "pad_layouts.h"
#include "retropad.h"
#include "settings_file.h"
#include "touch_fingers.h"

union SDL_Event;
struct ImFont;

enum class OnscreenPadShow
{
    automatic,
    on,
    off,
};

struct OnscreenPad
{
    TouchFingers fingers;
    PadLayouts   layouts;
    PadEditor    editor;
    Haptics      haptics;
    SettingsFile file;          // touch_layouts.cfg
    std::string  path;
    RetroPad     pad;           // what the fingers held at the last draw: the router's next frame
    bool         plugged = false;
    uint32_t     device = 0;    // in the router while plugged
    bool         hidden = false;   // AUTO: a real gamepad played (a tap shows it again)
    bool         touch_screen = false;   // there is one (at start, or a finger came since)
    bool         menu_pressed = false;   // MENU this frame
    bool         edit_done = false;      // EDIT LAYOUT's DONE this frame
    // Where it goes this frame (onscreen_pad_place): under the picture
    // (`below`, the picture keeps `picture_height` window pixels at the top)
    // or over it.
    bool         shown = false;
    bool         below = false;
    float        picture_height = 0.0f;
    PadLayoutId  layout = PadLayoutId::remote_overlay_landscape;   // the one shown
};

// Loads the layouts (`settings_dir` "" = nothing saved) and opens the vibrator.
void onscreen_pad_init(OnscreenPad* pad, const char* settings_dir);
void onscreen_pad_shutdown(OnscreenPad* pad);

// Every SDL event (fingers; ImGui's display size for their positions).
void onscreen_pad_event(OnscreenPad* pad, const SDL_Event* event, float display_width, float display_height);

// SHOW resolved: AUTO = a touch screen (seen at start, or a finger since).
bool onscreen_pad_enabled(const OnscreenPad* pad, OnscreenPadShow show);

// EDIT LAYOUT off (Escape, DONE): saved.
void onscreen_pad_edit_end(OnscreenPad* pad);

// Every frame before input_update: in the router while `enabled` (plugged /
// unplugged), what the fingers held handed over while `playing` (else
// nothing held); AUTO hides it when a real gamepad plays.
void onscreen_pad_update(OnscreenPad* pad, InputSystem* input, bool enabled, OnscreenPadShow show, bool playing);

// Before the game is drawn: shown or not (`playing`: a game runs with no
// menu up, or EDIT LAYOUT), and where, for a picture of `aspect` (width /
// height) in a `width` x `height` window.
void onscreen_pad_place(OnscreenPad* pad, bool playing, float aspect, float width, float height);

// The pad (or EDIT LAYOUT), over the game. `opacity` 0.1..1 (the layouts not
// changed in EDIT LAYOUT); `vibrate`: a buzz on each press.
void onscreen_pad_draw(OnscreenPad* pad, ImFont* pixel_font, const char* menu_label, float opacity, bool vibrate);

// After the frame: the fingers' frame ends.
void onscreen_pad_end_frame(OnscreenPad* pad);

// EDIT LAYOUT on (the layout shown now); saved when DONE.
void onscreen_pad_edit(OnscreenPad* pad);
