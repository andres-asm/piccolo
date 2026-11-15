#pragma once

// piccolo's "big picture" UI: full screen, made for a gamepad on a TV
// (Android, consoles, a couch PC), in the retro style (decided by Andrés,
// 2026-11-14/15). Chosen with SETTINGS > INTERFACE > UI MODE; the desktop
// menu bar stays the default.
//
// First screen: HOME, shown while no game runs.
//
//              [icon] piccolo             (assets/branding logo; the pixel
//                                          font's PICCOLO if it's missing)
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
#include <vector>

#include "localization.h"
#include "vk_texture.h"

struct AppShell;

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

    // The logo (assets/branding/png/piccolo-logo-dark.png next to the
    // program), loaded once, uploaded on the next frame.
    bool                  logo_tried = false;
    std::vector<uint32_t> logo_pixels;
    uint32_t              logo_width = 0;
    uint32_t              logo_height = 0;
    StreamingTexture      logo_texture;
    uint64_t              logo_texture_id = 0;
    bool                  logo_upload_pending = false;
    bool                  logo_ready = false;
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

// Uploads the logo once it's loaded. Call after app_shell_acquire, outside rendering.
void big_picture_home_upload(BigPictureHome* home, AppShell* shell);

// Frees the logo's texture (waits for the GPU).
void big_picture_home_destroy(BigPictureHome* home, AppShell* shell);

// Draws HOME over the whole window (behind any other window). Returns what
// was picked this frame.
BigPictureAction big_picture_home_draw(BigPictureHome* home, const BigPictureContext* context);
