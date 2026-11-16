#pragma once

// MAP CONTROLLER (milestone 3, step 5c): a modal that maps a controller as
// it is, Steam style:
//
//   OFFER        "NEW CONTROLLER: <name>" the first time one is used (or
//                when SDL doesn't know it): MAP / NOT NOW (30 s, then NOT NOW).
//   APPEARANCE   MAKER and MODEL, two linked lists of the layouts
//                (layouts/<Maker>/<model>.cfg), preselected from SDL's type:
//                they give the names of the buttons and which ones it has.
//   MAPPING      a drawn pad with the button to press lit; PRESS <its name>;
//                the optional ones skip after 5 s; BACK / SKIP / CANCEL.
//
// The result is an SDL mapping for the controller's GUID (kept in
// settings/gamepad_mappings.txt) and its appearance in its profile
// (input.cfg). While mapping, the input router is suspended, so presses do
// nothing else.

#include <cstdint>
#include <string>

#include "input_bindings.h"
#include "input_layouts.h"
#include "input_sdl_mapping.h"
#include "localization.h"

struct ImFont;

enum class MapStage
{
    closed,
    offer,
    appearance,
    mapping,
};

struct MapWindow
{
    MapStage       stage = MapStage::closed;
    uint32_t       joystick_id = 0;
    std::string    name;
    std::string    guid;
    bool           needed = false;           // SDL doesn't know it: unusable until mapped
    std::string    layout = "Generic/generic";   // the appearance picked (an InputLayout id)
    uint64_t       offer_started_ms = 0;
    InputSdlMapper mapper;
    bool           profiles_dirty = false;   // an appearance (or NOT NOW) was kept: save input.cfg
    std::string    result;                   // "<name>: MAPPED" (or refused) for the status line, once
    bool           take_focus = false;       // just opened: come to the front once (every frame would close its lists)
};

struct MapWindowContext
{
    InputSystem*        input = nullptr;
    const Localization* strings = nullptr;
    const char*         mappings_path = "";
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
