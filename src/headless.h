#pragma once

// GO HEADLESS (Andrés, 2026-11-20): a device in remote play takes the game.
// piccolo's window goes away while the game keeps running for the stream:
// the window is hidden and a tray icon (SHOW PICCOLO, PLAY HERE AGAIN) brings
// it back. Where there's no tray (GNOME without the AppIndicator extension,
// for one) the window is minimized instead, so the taskbar brings it back.
//
// Only the window and the tray here. main.cpp does the rest while `on`: the
// loop runs off the Exact timer and doesn't present (a hidden window gets no
// frame callbacks on Wayland: presenting would stall the game and the
// stream), local audio is muted, menus don't pause the game; and it comes
// back when the device asks, leaves, stops streaming or loses STREAM access
// (then the game pauses), or from the tray.

#include <cstdint>

#include "app_shell.h"

struct SDL_Tray;
struct SDL_Window;

struct Headless
{
    bool      on = false;
    uint32_t  peer = 0;            // the device that took the game (a NetAccess peer)
    SDL_Tray* tray = nullptr;      // while on, if the desktop has a tray
    bool      minimized = false;   // no tray: minimized instead of hidden
    bool      back_asked = false;  // SHOW PICCOLO / PLAY HERE AGAIN (set by the tray's callbacks)
};

// Hides the window behind a tray icon (or minimizes it). `icon` is one PNG of
// the app's icon; `show` / `play_here` / `tooltip` are the tray's texts.
void headless_enter(Headless* headless, SDL_Window* window, uint32_t peer, const AppShellIcon* icon,
                    const char* tooltip, const char* show, const char* play_here);
// Shows the window again and removes the tray icon.
void headless_leave(Headless* headless, SDL_Window* window);
