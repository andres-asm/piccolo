#pragma once

// What has piccolo's screen and input: one mode at a time (docs/design-notes.md,
// "UI modes" and "What pauses a core", decided 2026-11-21). No ImGui and no
// SDL here, so the rules can be tested alone (tools/ui_mode_test.cpp);
// main.cpp opens and closes the windows themselves.
//
//   GAME        the game has the input (the on-screen pad too).
//   QUICK MENU  the actions on the running core (a slide-out on the right).
//   WIDGET      one window widget during play: the ports popup (DEVICES),
//               a core's text box, the shader KEEP / REVERT question, the
//               DVR's GUI. HUD widgets (the ports' tabs, indicators) are no
//               mode: they draw over anything and take no input.
//   MAIN MENU   the menu bar and its stack of windows (settings, LOAD GAME...);
//               always this mode while no game runs.
//
// The mode changes only through the ui_enter_* functions. Each change counts
// in UiState::changes: main.cpp then lets go of every held key and cancels
// any input grab (SET, MAP CONTROLLER, the macro recorder).
//
// Everything that depends on the mode comes from ui_derive(), in one place:
// the input router's layer, the hotkeys' modifier, the on-screen pad, the
// pause and its causes, and when AUTO FRAME DELAY stops measuring.

#include <cstdint>

enum class UiMode
{
    game,
    quick_menu,
    widget,
    main_menu,
};

enum class UiWidget
{
    none,
    ports,          // DEVICES (the ports widget's popup)
    text_input,     // a core's TEXT request (libretro extension); the core waits for it
    shader_check,   // KEEP / REVERT a preset that's too slow; it measures the frame time
    dvr,            // the DVR's GUI (piccolo-rewind)
};

// MAIN MENU's windows. Each is drawn by its own module; main.cpp keeps the
// modules' open flags and this stack in step (a window closing itself with
// its X leaves the stack).
enum class UiWindow
{
    none,
    load_game,
    choose_folder,     // CHOOSE FOLDER (the file picker, from SETTINGS)
    settings,
    image_settings,
    core_options,      // CORE OPTIONS MANAGER
    cores,             // TOOLS > CORES
    share,             // TOOLS > SHARE
    input,             // SETTINGS > INPUT
    map_controller,    // MAP CONTROLLER
    safe_start,        // SAFE START (after a crash, before the game starts)
    debug,             // the debug window
    pad_editor,        // the on-screen pad's EDIT LAYOUT (drawn by the overlay)
    count,
};

constexpr uint32_t UI_WINDOW_MAX = static_cast<uint32_t>(UiWindow::count);

// Input grabs: a control is being read raw for a binding. Owned by the mode
// (and MAIN MENU's window) that took it, cancelled when that ends.
enum class UiGrab
{
    none,
    capture,   // SET (INPUT, QUICK MENU > CONTROLS): input_capture_*
    mapping,   // MAP CONTROLLER: the router suspended
    macro,     // the macro recorder
};

struct UiState
{
    UiMode   mode = UiMode::main_menu;
    UiWidget widget = UiWidget::none;          // mode == widget only
    UiWindow windows[UI_WINDOW_MAX] = {};      // MAIN MENU's stack, bottom first
    uint32_t window_count = 0;
    bool     game_running = false;             // set by ui_game_started / ui_game_stopped
    UiGrab   grab = UiGrab::none;
    UiMode   grab_mode = UiMode::main_menu;    // its owner...
    UiWindow grab_window = UiWindow::none;     // ...and MAIN MENU's window that took it (none: the mode itself)
    bool     user_paused = false;              // the USER pause cause: only the user lifts it
    uint32_t changes = 0;                      // counts mode changes
};

// A game started (GAME, everything closed, no USER pause: it was the old
// game's) or stopped (MAIN MENU). Stopping again does nothing.
void ui_game_started(UiState* state);
void ui_game_stopped(UiState* state);

// The transitions. True when the mode (or the widget) changed. Entering a
// mode closes what the old one had open (MAIN MENU's stack, the widget).
// GAME and QUICK MENU need a running game (refused otherwise).
bool ui_enter_game(UiState* state);
bool ui_enter_quick_menu(UiState* state);
bool ui_enter_widget(UiState* state, UiWidget widget);
bool ui_enter_main_menu(UiState* state);

// MAIN MENU's stack. Pushing enters MAIN MENU (from any mode); a window
// already open moves to the top. Removing keeps MAIN MENU (the bar alone).
void     ui_push_window(UiState* state, UiWindow window);
void     ui_remove_window(UiState* state, UiWindow window);
bool     ui_window_open(const UiState* state, UiWindow window);
UiWindow ui_top_window(const UiState* state);

// A grab starts (owned by the mode and MAIN MENU's top window now) or ends.
// Orphaned: its owner is gone; main.cpp cancels it (the lockout guard).
void ui_grab_begin(UiState* state, UiGrab grab);
void ui_grab_end(UiState* state);
bool ui_grab_orphaned(const UiState* state);

// Esc / B ("back"): UI navigation, never a hotkey, no modifier in any mode.
enum class UiBack
{
    none,             // GAME: the key is the game's
    cancel_grab,      // a grab is on: it's cancelled, nothing closes
    close_sub_view,   // QUICK MENU's shader picker or remap view
    close_widget,     // the widget's own back (text box: cancel; KEEP / REVERT: REVERT), then GAME
    close_window,     // MAIN MENU's top window
    to_game,          // QUICK MENU, or the bar alone with a game running
};

UiBack ui_back(const UiState* state, bool quick_menu_sub_view);

// The pause causes (a bit each; the pause is their sum).
enum class UiPause
{
    menu,          // a window that isn't a HUD, with PAUSE IN MENU
    focus,         // the window lost focus, with PAUSE ON FOCUS LOSS
    controllers,   // every controller gone, with PAUSE WHEN CONTROLLERS DISCONNECT
    user,          // the PAUSE hotkey / button / menu item, a device's PAUSE
    background,    // Android: the app is in the background
    count,
};

uint32_t ui_pause_bit(UiPause cause);

// What ui_derive() needs besides the state: settings and facts of this frame.
struct UiFacts
{
    bool pause_in_menu = false;         // PAUSE IN MENU
    bool pause_on_focus_loss = false;   // PAUSE ON FOCUS LOSS
    bool pause_on_disconnect = false;   // PAUSE WHEN CONTROLLERS DISCONNECT
    bool focused = true;
    bool streaming = false;             // a device streams the game (whoever plays looks at it)
    bool headless = false;              // GO HEADLESS: nobody sees this window's menus
    bool played_with_gamepad = false;   // the last game input came from a local gamepad
    bool gamepad_on_port = false;       // a local gamepad is on a port now
    bool background = false;            // Android: in the background
};

struct UiDerived
{
    bool     interface_layer = false;         // the router's INTERFACE layer: the core gets no input
    bool     keyboard_hotkeys_free = false;   // the keyboard's hotkeys without the modifier (MAIN MENU)
    bool     overlay_active = false;          // the on-screen pad takes touches (GAME only)
    uint32_t pause = 0;                       // UiPause bits; 0 = runs
    bool     frame_delay_interrupt = false;   // AUTO FRAME DELAY stops measuring
};

UiDerived ui_derive(const UiState* state, const UiFacts* facts);
