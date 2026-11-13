#include "menus.h"

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
    { Str::menu_video,   MenuAction::video },
    { Str::menu_input,   MenuAction::input },
    { Str::menu_sound,   MenuAction::sound },
};

static const MenuItem MISC_ITEMS[] = {
    { Str::menu_debug_window, MenuAction::debug_window },
    { Str::menu_about,        MenuAction::about },
};

static const Menu MENUS[] = {
    { Str::menu_game,   GAME_ITEMS,   static_cast<int32_t>(sizeof(GAME_ITEMS) / sizeof(GAME_ITEMS[0])) },
    { Str::menu_config, CONFIG_ITEMS, static_cast<int32_t>(sizeof(CONFIG_ITEMS) / sizeof(CONFIG_ITEMS[0])) },
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
        case MenuAction::video:      return "VIDEO";
        case MenuAction::sound:      return "SOUND";
        case MenuAction::input:      return "INPUT";
        case MenuAction::about:      return "ABOUT";
        case MenuAction::debug_window: return "DEBUG WINDOW";
        default:                     return "?";
    }
}
