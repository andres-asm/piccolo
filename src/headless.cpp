#include "headless.h"

#include <SDL3/SDL.h>

// The tray's entries call back on the main thread (while SDL pumps events):
// they only ask, main.cpp leaves headless at the top of its next loop.
static void SDLCALL on_back(void* userdata, SDL_TrayEntry* entry)
{
    (void)entry;
    static_cast<Headless*>(userdata)->back_asked = true;
}

static SDL_Tray* create_tray(Headless* headless, const AppShellIcon* icon, const char* tooltip, const char* show,
                             const char* play_here)
{
    SDL_Surface* surface = nullptr;
    if (icon != nullptr)
    {
        SDL_IOStream* io = SDL_IOFromConstMem(icon->png, icon->size);
        surface = io != nullptr ? SDL_LoadPNG_IO(io, true) : nullptr;
    }
    SDL_Tray* tray = SDL_CreateTray(surface, tooltip);
    if (surface != nullptr)
    {
        SDL_DestroySurface(surface);   // the tray keeps its own copy
    }
    if (tray == nullptr)
    {
        return nullptr;
    }
    SDL_TrayMenu* menu = SDL_CreateTrayMenu(tray);
    SDL_TrayEntry* show_entry = menu != nullptr ? SDL_InsertTrayEntryAt(menu, -1, show, SDL_TRAYENTRY_BUTTON) : nullptr;
    SDL_TrayEntry* play_entry =
        menu != nullptr ? SDL_InsertTrayEntryAt(menu, -1, play_here, SDL_TRAYENTRY_BUTTON) : nullptr;
    if (show_entry == nullptr || play_entry == nullptr)
    {
        SDL_DestroyTray(tray);   // a tray without its menu couldn't bring the window back
        return nullptr;
    }
    SDL_SetTrayEntryCallback(show_entry, on_back, headless);
    SDL_SetTrayEntryCallback(play_entry, on_back, headless);
    return tray;
}

void headless_enter(Headless* headless, SDL_Window* window, uint32_t peer, const AppShellIcon* icon,
                    const char* tooltip, const char* show, const char* play_here)
{
    if (headless->on)
    {
        return;
    }
    headless->on = true;
    headless->peer = peer;
    headless->back_asked = false;
    headless->tray = create_tray(headless, icon, tooltip, show, play_here);
    headless->minimized = headless->tray == nullptr;
    if (headless->minimized)
    {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Headless: no tray (%s): minimizing instead", SDL_GetError());
        SDL_MinimizeWindow(window);
    }
    else
    {
        SDL_HideWindow(window);
    }
}

void headless_leave(Headless* headless, SDL_Window* window)
{
    if (!headless->on)
    {
        return;
    }
    if (headless->tray != nullptr)
    {
        SDL_DestroyTray(headless->tray);
        headless->tray = nullptr;
    }
    if (headless->minimized)
    {
        SDL_RestoreWindow(window);
    }
    else
    {
        SDL_ShowWindow(window);
    }
    SDL_RaiseWindow(window);
    headless->on = false;
    headless->minimized = false;
    headless->back_asked = false;
}
