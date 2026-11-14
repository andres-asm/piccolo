#pragma once

// piccolo's settings window, in the retro style:
//
//   +- SETTINGS ------------------------------------------+
//   | SCOPE [ THIS CORE (SNES9X)                       v ] |   only while a game runs
//   | [GENERAL] [VIDEO] [SOUND]                           |
//   |   [PICTURE] [DISPLAY] [SYNC]                        |
//   |   ASPECT RATIO  [ CORE PROVIDED            v ]      |
//   | C SCALING       [ INTEGER (OVER)           v ] [RESET]
//   |   ...                                               |
//   |   [IMAGE SETTINGS...]                               |
//   +-----------------------------------------------------+
//
// The scope says which layer changes go to: GLOBAL, THIS CORE or THIS GAME
// (plan-settings: avoids "which layer did my change go to"). When the window
// opens it starts at the most specific layer already in use (Andrés,
// 2026-11-13): a game with its own settings opens on THIS GAME, else a core
// with its own on THIS CORE, else GLOBAL. Changes keep going to the override.

#include <cstdint>

#include "app_paths.h"
#include "image_window.h"
#include "localization.h"
#include "settings.h"

struct ImFont;

enum class SettingsTab
{
    general,
    video,
    sound,
};

struct SettingsWindow
{
    bool         open        = false;
    bool         image_open  = false;   // the IMAGE SETTINGS window
    SettingsTab  tab         = SettingsTab::general;
    bool         select_tab  = false;   // select `tab` on the next draw
    SettingLayer scope       = SettingLayer::global;
    bool         choose_scope = false;  // pick the scope on the next draw (just opened)
};

// What the window needs from piccolo this frame.
struct SettingsWindowContext
{
    Settings*                settings = nullptr;
    const Localization*      strings = nullptr;  // UI text (piccolo_text.h)
    const AppPaths*          paths = nullptr;    // the default folders, shown in empty path fields
    const char*              core_name = "";     // "" = no core running
    const char*              game_name = "";     // "" = no game (or running without content)
    const ImageWindowSource* image = nullptr;    // the game picture for IMAGE SETTINGS
    const char*              window_size = "";   // what the window mode gives, for the Display tab
    const char*              present_wait = "";  // how WAIT FOR PRESENT works on this GPU, for the Sync tab
    const char* const*       audio_devices = nullptr;   // playback devices connected now, for the Sound tab
    uint32_t                 audio_device_count = 0;
    const char*              audio_latency = "";  // the latency in use, for the Sound tab
    const char*              game_mode = "";      // GameMode's state, for the General tab
};

void settings_window_open(SettingsWindow* window, SettingsTab tab);

// Draws the window (and IMAGE SETTINGS) while open. The caller saves when
// both have closed (Esc may close them too, outside this function).
void settings_window_draw(SettingsWindow* window, const SettingsWindowContext* context, ImFont* pixel_font,
                          int32_t scale);
