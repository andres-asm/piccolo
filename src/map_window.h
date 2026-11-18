#pragma once

// MAP CONTROLLER (milestone 3, step 5c): a modal that maps a controller as
// it is, Steam style: a small wizard, its controller drawing always in the
// same place, a page indicator above its buttons, the buttons at the bottom.
//
//   OFFER    NEW CONTROLLER DETECTED the first time one is used (or when SDL
//            doesn't know it): MAP (or any button on it) / NOT NOW (15 s).
//   STEP 1   CONTROLLER TYPE: LABELS and LAYOUT (layouts/<Labels>/<layout>.cfg,
//            preselected from its profile or SDL's type) and its HOTKEY MODE.
//            START (10 s untouched) / SKIP (keep its type: only for a pad that
//            is already mapped) / CANCEL.
//   STEP 2   CONTROLLER BUTTONS: where each button is (the SDL mapping).
//            BACK / START (10 s) / SKIP (keep SDL's mapping) / CANCEL; then
//            each button lit on the drawing, held HOLD TO BIND (a ring closes).
//   STEP 3   USER INTERFACE: the button for each of its controls (navigate,
//            pages, accept, back, main menu, HOTKEY ENABLE, quick menu),
//            wherever it is, but one STEP 2 mapped. BACK / START / SKIP /
//            CANCEL, then held like STEP 2's. MAIN MENU also becomes HOTKEY
//            ENABLE + that button.
//   STEP 4   EXTRA BUTTONS (only from SETTINGS > INPUT > DEVICES > MAP...):
//            GUIDE / HOME?, paddles?, extra shoulders (L4 / R4)?, other
//            buttons? BACK / YES / NO (15 s) / CANCEL; then the ones answered
//            YES, held like STEP 2's (the others until none comes).
//   SUMMARY  what changes; RESTART / CANCEL / FINISH.
//
// While a step takes its inputs: BACK (the previous input), RESET (the
// step's first input again), SKIP (this input; by itself after TIME TO
// PRESS for an optional one). A wizard BACK goes back a whole step.
//
// Nothing is kept until FINISH: the SDL mapping is given to SDL as it is
// made (the later steps need the controller to work), and CANCEL (or any way
// out) puts SDL's mapping back as it was. FINISH writes the controller's
// profile (input.cfg): its mapping, type, hotkey mode and interface
// buttons. While inputs are taken the input router is suspended.
//
// A controller SDL doesn't know can't drive the interface: any button on it
// is the main button on OFFER, STEP 1 and the steps' first pages.

#include <cstdint>
#include <string>

#include "input_bindings.h"
#include "input_layouts.h"
#include "input_sdl.h"
#include "input_sdl_mapping.h"
#include "localization.h"

struct ImFont;

enum class MapStage
{
    closed,
    offer,
    type,              // STEP 1
    intro_buttons,     // STEP 2's first page
    mapping,           // STEP 2's inputs, then STEP 3's
    intro_interface,   // STEP 3's first page (the mapper waits meanwhile)
    extras_guide,      // STEP 4's questions (from SETTINGS only)
    extras_paddles,
    extras_shoulders,
    extras_others,
    extras_mapping,    // STEP 4's inputs
    summary,
};

constexpr uint32_t MAP_WINDOW_ROLES_MAX = 16;

struct MapWindow
{
    MapStage       stage = MapStage::closed;
    uint32_t       joystick_id = 0;
    std::string    name;
    std::string    guid;
    bool           needed = false;           // SDL doesn't know it: unusable until mapped
    bool           offered = false;          // came as NEW CONTROLLER DETECTED (else MAP CONTROLLER, from DEVICES)
    std::string    layout = "Generic/generic";   // STEP 1's pick (an InputLayout id)
    InputHotkeyMode hotkey_mode = InputHotkeyMode::hold;
    std::string    initial_layout;           // what it opened with (RESTART)
    InputHotkeyMode initial_hotkey_mode = InputHotkeyMode::hold;
    uint64_t       offer_started_ms = 0;
    uint64_t       page_started_ms = 0;      // the current page's countdown
    MapStage       drawn_stage = MapStage::closed;   // the page drawn last (to notice entering one)

    // What FINISH keeps.
    bool           skip_type = false;        // STEP 1 skipped: its type and hotkey mode stay
    bool           skip_buttons = false;     // STEP 2 skipped: SDL's mapping stays (STEP 3 finds its buttons there)
    bool           roles_done = false;       // STEP 3 done (not skipped)
    InputSource    role_sources[MAP_WINDOW_ROLES_MAX] = {};
    std::string    pending_mapping;          // the SDL mapping made ("" = SDL's stays)
    std::string    original_mapping;         // SDL's when it opened ("" = none): CANCEL puts it back
    bool           mapping_changed = false;  // SDL has pending_mapping now
    bool           interface_intro_done = false;   // STEP 3's first page was shown
    bool           want_guide = false;       // STEP 4's answers
    bool           want_paddles = false;
    bool           want_shoulders = false;
    bool           want_others = false;
    uint32_t       extras_mapped = 0;        // STEP 4: how many extras it took

    InputSdlMapper mapper;
    InputSdlPress  press;                    // any button on it: the page's main button (it can't drive the interface)
    MapStage       press_stage = MapStage::closed;   // the page `press` last looked from (a new page waits for a release)
    bool           profiles_dirty = false;   // FINISH (or NOT NOW) changed a profile: save input.cfg
    std::string    result;                   // "<name>: MAPPED" for the status line, once
    bool           take_focus = false;       // just opened: come to the front once (every frame would close its lists)
};

struct MapWindowContext
{
    InputSystem*        input = nullptr;
    const Localization* strings = nullptr;
    const InputLayouts* layouts = nullptr;
    uint64_t            now_ms = 0;
};

// The OFFER for a controller (by its SDL joystick id). `needed`: SDL doesn't
// know it, so it can't be used unmapped.
void map_window_offer(MapWindow* window, uint32_t joystick_id, bool needed, const InputLayouts* layouts,
                      const InputSystem* input, uint64_t now_ms);

// Straight to STEP 1 (DEVICES > MAP...). The type and hotkey mode start as
// the controller's profile has them (or SDL's guess).
void map_window_open(MapWindow* window, uint32_t joystick_id, const InputLayouts* layouts, const InputSystem* input);

// Gives up (CANCEL, Escape, the controller went away): SDL's mapping as it
// was, nothing kept; the router comes back.
void map_window_close(MapWindow* window, InputSystem* input);

bool map_window_open_now(const MapWindow* window);

void map_window_draw(MapWindow* window, const MapWindowContext* context, ImFont* pixel_font, int32_t scale);
