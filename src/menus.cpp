#include "menus.h"

#include "pixel_font_imgui.h"   // PIXEL_FONT_UTF8_MENU
#include "retro_ui.h"

struct MenuItem
{
    Str         label;
    MenuAction  action;
};

struct Menu
{
    Str             label;
    const MenuItem* items;
    int32_t         item_count;
};

static const MenuItem GAME_ITEMS[] = {
    { Str::menu_load_game,  MenuAction::load_game },
    { Str::menu_reset,      MenuAction::reset },
    { Str::menu_save_state, MenuAction::save_state },
    { Str::menu_load_state, MenuAction::load_state },
    { Str::menu_quit,       MenuAction::quit },
};

static const MenuItem CONFIG_ITEMS[] = {
    { Str::menu_general, MenuAction::general },
    { Str::menu_interface, MenuAction::interface },
    { Str::menu_video,   MenuAction::video },
    { Str::menu_input,   MenuAction::input },
    { Str::menu_sound,   MenuAction::sound },
};

// Tools: the options manager now; a core installer later (Andrés, 2026-11-14).
static const MenuItem TOOLS_ITEMS[] = {
    { Str::menu_options_manager, MenuAction::options_manager },
};

static const MenuItem MISC_ITEMS[] = {
    { Str::menu_debug_window, MenuAction::debug_window },
    { Str::menu_about,        MenuAction::about },
};

static const Menu MENUS[] = {
    { Str::menu_game,   GAME_ITEMS,   static_cast<int32_t>(sizeof(GAME_ITEMS) / sizeof(GAME_ITEMS[0])) },
    { Str::menu_config, CONFIG_ITEMS, static_cast<int32_t>(sizeof(CONFIG_ITEMS) / sizeof(CONFIG_ITEMS[0])) },
    { Str::menu_tools,  TOOLS_ITEMS,  static_cast<int32_t>(sizeof(TOOLS_ITEMS) / sizeof(TOOLS_ITEMS[0])) },
    { Str::menu_misc,   MISC_ITEMS,   static_cast<int32_t>(sizeof(MISC_ITEMS) / sizeof(MISC_ITEMS[0])) },
};

constexpr int32_t MENU_COUNT = static_cast<int32_t>(sizeof(MENUS) / sizeof(MENUS[0]));

int32_t menus_count()
{
    return MENU_COUNT;
}

Str menus_label(int32_t menu)
{
    return MENUS[menu].label;
}

int32_t menus_item_count(int32_t menu)
{
    return MENUS[menu].item_count;
}

Str menus_item_label(int32_t menu, int32_t item)
{
    return MENUS[menu].items[item].label;
}

MenuAction menus_item_action(int32_t menu, int32_t item)
{
    return MENUS[menu].items[item].action;
}

// The bar's item ids: a MenuAction's number, or RECENT_ID_FIRST + the index
// of a recent game.
constexpr int32_t RECENT_ID_FIRST = 1000;
// Room for the bar's labels, built every frame on the stack (no allocation).
constexpr int32_t BAR_ITEMS_MAX = 16;

MenuAction menus_bar(ImFont* pixel_font, int32_t scale, const Localization* strings, const char* const* recent,
                     int32_t recent_count, int32_t* recent_picked, bool open_first_menu)
{
    *recent_picked = -1;
    RetroUiMenuItem items[1 + MENU_COUNT][BAR_ITEMS_MAX];
    RetroUiMenu menus[1 + MENU_COUNT];

    int32_t shown_recent = recent_count < BAR_ITEMS_MAX ? recent_count : BAR_ITEMS_MAX;
    for (int32_t i = 0; i < shown_recent; i++)
    {
        items[0][i] = {recent[i], RECENT_ID_FIRST + i};
    }
    menus[0] = {PIXEL_FONT_UTF8_MENU, items[0], shown_recent, piccolo_text(strings, Str::menu_recent_empty)};

    for (int32_t m = 0; m < MENU_COUNT; m++)
    {
        const Menu* menu = &MENUS[m];
        int32_t count = menu->item_count < BAR_ITEMS_MAX ? menu->item_count : BAR_ITEMS_MAX;
        for (int32_t i = 0; i < count; i++)
        {
            MenuAction action = menu->items[i].action;
            int32_t id = action == MenuAction::separator ? RETRO_UI_MENU_SEPARATOR : static_cast<int32_t>(action);
            items[1 + m][i] = {piccolo_text(strings, menu->items[i].label), id};
        }
        menus[1 + m] = {piccolo_text(strings, menu->label), items[1 + m], count, nullptr};
    }

    int32_t picked = retro_ui_menu_bar(pixel_font, scale, menus, 1 + MENU_COUNT, open_first_menu);
    if (picked == RETRO_UI_MENU_NONE)
    {
        return MenuAction::none;
    }
    if (picked >= RECENT_ID_FIRST)
    {
        *recent_picked = picked - RECENT_ID_FIRST;
        return MenuAction::recent_game;
    }
    return static_cast<MenuAction>(picked);
}

const char* menu_action_name(MenuAction action)
{
    switch (action)
    {
        case MenuAction::none:       return "NONE";
        case MenuAction::load_game:  return "LOAD GAME";
        case MenuAction::reset:      return "RESET";
        case MenuAction::save_state: return "SAVE STATE";
        case MenuAction::load_state: return "LOAD STATE";
        case MenuAction::quit:       return "QUIT";
        case MenuAction::general:    return "GENERAL";
        case MenuAction::interface:  return "INTERFACE";
        case MenuAction::video:      return "VIDEO";
        case MenuAction::sound:      return "SOUND";
        case MenuAction::input:      return "INPUT";
        case MenuAction::about:      return "ABOUT";
        case MenuAction::debug_window: return "DEBUG WINDOW";
        case MenuAction::recent_game:  return "RECENT GAME";
        case MenuAction::options_manager: return "OPTIONS MANAGER";
        case MenuAction::separator:    return "SEPARATOR";
        default:                     return "?";
    }
}
