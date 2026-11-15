#pragma once

// piccolo's "big picture" UI: full screen, made for a gamepad on a TV
// (Android, consoles, a couch PC), in the retro style (decided by Andrés,
// 2026-11-14/15). Chosen with SETTINGS > INTERFACE > UI MODE; the desktop
// menu bar stays the default.
//
// First screen: HOME, shown while no game runs.
//
//                  * PICCOLO *            (logo: the pixel font, big)
//                LIBRETRO PLAYER
//             +------------------+
//             | > LOAD GAME      |        up/down (d-pad, stick, arrows) move,
//             |   LOAD CORE      |        confirm (south button / Return) picks,
//             |   SETTINGS       |        the mouse hovers and clicks
//             |   QUIT           |
//             +------------------+
//          CORE SNES9X - LAST USED
//              NO GAME LOADED
//   +-----------------------------------------------+
//   | V0.1-DEV              [dpad] MOVE  [A] SELECT  |  hints for the device used last
//   +-----------------------------------------------+
//
// Over a starry sky with a city skyline, in the theme's colors.

#include <cstdint>

#include "localization.h"

struct ImFont;
struct InputGlyphs;
struct RetroUiPalette;

constexpr const char* PICCOLO_VERSION = "0.1-dev";

enum class BigPictureAction
{
    none,
    load_game,
    load_core,
    settings,
    quit,
};

struct BigPictureHome
{
    int32_t selected = 0;   // the highlighted item
};

struct BigPictureContext
{
    const Localization* strings = nullptr;
    const InputGlyphs*  glyphs = nullptr;
    const RetroUiPalette* palette = nullptr;   // the theme's colors
    ImFont*             pixel_font = nullptr;
    int32_t             scale = 1;            // window pixels per retro pixel
    const char*         core_name = "";       // the last used core ("" = none yet)
    bool                takes_input = true;   // false while another window (LOAD GAME, SETTINGS) is on top
};

// Draws HOME over the whole window (behind any other window). Returns what
// was picked this frame.
BigPictureAction big_picture_home_draw(BigPictureHome* home, const BigPictureContext* context);
