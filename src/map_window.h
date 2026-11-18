#pragma once

// MAP CONTROLLER (milestone 3, step 5c): a modal that maps a controller as
// it is, Steam style:
//
// a small wizard (its buttons always at the bottom):
//
//   OFFER        NEW CONTROLLER DETECTED the first time one is used (or
//                when SDL doesn't know it): MAP (or any button on it) / NOT NOW
//                (15 s, then NOT NOW).
//   APPEARANCE   SELECT CONTROLLER TYPE: LABELS and LAYOUT, two linked lists
//                of the layouts (layouts/<Labels>/<layout>.cfg), preselected
//                from SDL's type: they give the names of the buttons and which
//                ones it has. Also its HOTKEY MODE (HOLD by default). START by
//                itself after 15 s untouched (the guess is usually right).
//   STEP 1       CONTROLLER BUTTONS: what comes, with the pad, then START
//                (by itself after 10 s, or any button on it); then the drawn
//                pad with the button to press lit; PRESS <its name>; the
//                optional ones skip after 5 s; BACK / SKIP / CANCEL. This is
//                the SDL mapping: where each button is.
//   STEP 2       USER INTERFACE: what comes, then START (the same way); the
//                button for each of its controls (navigate, pages, accept,
//                back, main menu, HOTKEY ENABLE, quick menu), wherever it is,
//                but only one STEP 1 mapped, held HOLD TO BIND (a bar fills; let go
//                sooner and it isn't taken); the drawn pad lights the one
//                held. MAIN MENU also becomes HOTKEY ENABLE + that button.
//
// A controller SDL doesn't know can't drive the interface: on the type's page
// any button on it is the main button; on OFFER and both steps' pages any
// button on the controller being mapped is the main button, for every
// controller.
//
// The result is an SDL mapping for the controller's GUID and its appearance,
// both in its profile (input.cfg), with the interface's buttons as changes to its bindings
// (applied to it now, if it is in use). While mapping, the input router is suspended, so presses do
// nothing else.

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
    appearance,
    intro_buttons,     // STEP 1's page, before its presses
    mapping,           // STEP 1's presses, then STEP 2's
    intro_interface,   // STEP 2's page (the mapper waits meanwhile)
};

struct MapWindow
{
    MapStage       stage = MapStage::closed;
    uint32_t       joystick_id = 0;
    std::string    name;
    std::string    guid;
    bool           needed = false;           // SDL doesn't know it: unusable until mapped
    std::string    layout = "Generic/generic";   // the appearance picked (an InputLayout id)
    InputHotkeyMode hotkey_mode = InputHotkeyMode::hold;
    uint64_t       offer_started_ms = 0;
    uint64_t       intro_started_ms = 0;
    uint64_t       appearance_started_ms = 0;   // SELECT CONTROLLER TYPE: START by itself after a while untouched
    MapStage       drawn_stage = MapStage::closed;   // the page drawn last (to notice entering one)
    bool           offered = false;          // came as NEW CONTROLLER DETECTED (else MAP CONTROLLER, from DEVICES)
    bool           interface_intro_done = false;  // STEP 2's page was shown
    InputSdlMapper mapper;
    InputSdlPress  press;                    // any button on it: the page's main button (it can't drive the interface)
    MapStage       press_stage = MapStage::closed;   // the page `press` last looked from (a new page waits for a release)
    bool           profiles_dirty = false;   // a mapping, an appearance (or NOT NOW) was kept: save input.cfg
    std::string    result;                   // "<name>: MAPPED" (or refused) for the status line, once
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
void map_window_offer(MapWindow* window, uint32_t joystick_id, bool needed, const InputLayouts* layouts, uint64_t now_ms);

// Straight to APPEARANCE (DEVICES > MAP...).
void map_window_open(MapWindow* window, uint32_t joystick_id, const InputLayouts* layouts);

// Gives up (Escape, or the controller went away); the router comes back.
void map_window_close(MapWindow* window, InputSystem* input);

bool map_window_open_now(const MapWindow* window);

void map_window_draw(MapWindow* window, const MapWindowContext* context, ImFont* pixel_font, int32_t scale);
