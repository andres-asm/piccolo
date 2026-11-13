#pragma once

// piccolo's settings window, in the retro style:
//
//   +- SETTINGS ------------------------------------------+
//   | SCOPE [ THIS CORE (SNES9X)                       v ] |   only while a game runs
//   | [GENERAL] [VIDEO]                                   |
//   |   [PICTURE] [SYNC]                                  |
//   |   ASPECT RATIO  [ CORE PROVIDED            v ]      |
//   | C SCALING       [ INTEGER (OVER)           v ] [RESET]
//   |   ...                                               |
//   |   [IMAGE SETTINGS...]                               |
//   +-----------------------------------------------------+
//
// The scope says which layer changes go to: GLOBAL, THIS CORE or THIS GAME
// (plan-settings: avoids "which layer did my change go to"). It starts at
// GLOBAL every time the window opens. The Display tab (window and fullscreen
// modes) arrives with plan-settings step 7, the low latency group with step 8.

#include <cstdint>

#include "app_paths.h"
#include "image_window.h"
#include "settings.h"

struct ImFont;

enum class SettingsTab
{
    general,
    video,
};

struct SettingsWindow
{
    bool         open        = false;
    bool         image_open  = false;   // the IMAGE SETTINGS window
    SettingsTab  tab         = SettingsTab::general;
    bool         select_tab  = false;   // select `tab` on the next draw
    SettingLayer scope       = SettingLayer::global;
};

// What the window needs from piccolo this frame.
struct SettingsWindowContext
{
    Settings*                settings = nullptr;
    const AppPaths*          paths = nullptr;    // the default folders, shown in empty path fields
    const char*              core_name = "";     // "" = no core running
    const char*              game_name = "";     // "" = no game (or running without content)
    const ImageWindowSource* image = nullptr;    // the game picture for IMAGE SETTINGS
};

void settings_window_open(SettingsWindow* window, SettingsTab tab);

// Draws the window (and IMAGE SETTINGS) while open. The caller saves when
// both have closed (Esc may close them too, outside this function).
void settings_window_draw(SettingsWindow* window, const SettingsWindowContext* context, ImFont* pixel_font,
                          int32_t scale);
