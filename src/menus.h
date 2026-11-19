#pragma once

// What piccolo's menus contain: menus, their items and the action each item
// triggers. Data, plus menus_bar(): retroui's menu bar with these menus. A TEST set for now: most actions
// only report that they were picked.

#include <cstdint>

#include "localization.h"
#include "piccolo_text.h"   // Str

struct ImFont;

enum class MenuAction
{
    none,
    load_game,
    reset,
    save_state,
    load_state,
    quit,
    general,
    interface,
    video,
    sound,
    input,
    about,
    debug_window,
    recent_game,   // from the RECENT menu (see retro_ui_menu_bar)
    options_manager,   // TOOLS > OPTIONS MANAGER (stored core options)
    separator,     // not an action: a line between items (its label isn't shown)
};

int32_t     menus_count();
Str         menus_label(int32_t menu);
int32_t     menus_item_count(int32_t menu);
Str         menus_item_label(int32_t menu, int32_t item);
MenuAction  menus_item_action(int32_t menu, int32_t item);

// The menu bar (retro_ui_menu_bar) with piccolo's menus in the language of
// `strings`. Returns the item picked this frame (MenuAction::none if none).
// The first menu (a "hamburger" glyph) lists the recent games: `recent`
// holds their labels, most recent first. Picking one returns
// MenuAction::recent_game and its index in *recent_picked.
// `open_first_menu`: open that first menu now, with the keyboard / gamepad
// focus on its first item (to start navigating without a mouse).
MenuAction menus_bar(ImFont* pixel_font, int32_t scale, const Localization* strings, const char* const* recent,
                     int32_t recent_count, int32_t* recent_picked, bool open_first_menu);

// "LOAD GAME" etc., for logs (English on purpose).
const char* menu_action_name(MenuAction action);
