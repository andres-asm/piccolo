#pragma once

// The rewind DVR in piccolo (Andrés, 2026-11-20 / 21; docs/design-notes.md
// "Rewind / DVR" and "UI modes"):
//
//   - The history: frontend-libretro's core_rewind, recorded by the core
//     session around every frame. Started once the game's settings are
//     known (dvr_widget_game_started): GENERAL > REWIND, and AUTO's earlier
//     verdict for the game (REWIND KNOWN, in the game's layer).
//   - The GUI is a window widget: it takes the input (one window at a time,
//     opening it closes the QUICK MENU) and the game stops while it's open
//     (its own logic, not a pause cause). Drawn with retroui's dvr_view: the
//     game shrinks into the space above the strip.
//       LEFT / RIGHT   a frame (SCRUB) or a snapshot; held, faster
//       L / R          5 s (SCRUB)
//       B (accept)     PLAY FROM HERE: play on from the cursor
//       Y              WATCH: the recording plays / pauses
//       A (back)       BACK TO NOW (the game as it was), and Escape
//   - A HUD indicator (no input, never pauses): "PLAYING FROM -0:27.8" after
//     PLAY FROM HERE, or why the DVR didn't open.
//
// Kept behind these calls on purpose: main.cpp's UI state is being reworked
// (ui-state) and rewires them without touching this file.

#include <cstdint>
#include <vector>

#include "core_rewind.h"
#include "dvr_view.h"
#include "video_texture.h"

struct AppShell;
struct CoreSession;
struct ImFont;
struct InputFrame;
struct Localization;
struct Settings;

struct DvrWidget
{
    CoreRewind rewind;            // the history (CoreSession::rewind points here)
    bool       open = false;

    // The strip.
    DvrView               view;
    std::vector<uint64_t> previews;   // frames with a thumbnail (every 5 s in SCRUB, every snapshot); built at open
    uint64_t              preview_frames = 1;   // frames between two previews
    VideoTexture          thumbs[DVR_VIEW_MAX_THUMBS];
    uint64_t              thumb_frame[DVR_VIEW_MAX_THUMBS] = {};   // the preview each holds
    bool                  thumb_used[DVR_VIEW_MAX_THUMBS] = {};

    // LEFT / RIGHT held: steps repeat, then faster.
    int32_t  held_direction = 0;
    uint64_t held_since_ms  = 0;
    uint64_t next_step_ms   = 0;

    // The HUD indicator.
    char     hud[96] = {};
    uint64_t hud_since_ms = 0;

    char readout[16] = {};
    char kept[32]    = {};
    char mode[48]    = {};
};

// The game just started (the session runs it, the settings' context is the
// game's): starts recording as GENERAL > REWIND says.
void dvr_widget_game_started(DvrWidget* widget, CoreSession* session, const Settings* settings);
// AUTO decided this frame: the mode to keep as the game's REWIND KNOWN
// (scrub / snapshots). False otherwise.
bool dvr_widget_take_verdict(DvrWidget* widget, RewindMode* known);

// Opens the GUI. False (and the HUD says why) if there's nothing to go back to.
bool dvr_widget_open(DvrWidget* widget, const Localization* strings, uint64_t now_ms);
// BACK TO NOW: closes, the game as it was.
void dvr_widget_close(DvrWidget* widget);

// While open, after input_update: the gamepads' and keyboard's actions.
void dvr_widget_update(DvrWidget* widget, const InputFrame* frame, const Localization* strings, uint64_t now_ms);

// While open, in a frame (after app_shell_acquire, before the session's own
// upload): makes the next missing thumbnail.
void dvr_widget_upload(DvrWidget* widget, CoreSession* session, AppShell* shell);

// Window pixels the strip takes at the bottom (0 when closed): the game is
// drawn above it.
float dvr_widget_strip_height(const DvrWidget* widget, int32_t scale);

// The strip (above every window) and what a click / tap on it did.
void dvr_widget_draw(DvrWidget* widget, ImFont* pixel_font, int32_t scale, const Localization* strings, bool touch,
                     uint64_t now_ms);
// The HUD indicator (behind the windows, drawn every frame).
void dvr_widget_draw_hud(DvrWidget* widget, ImFont* pixel_font, int32_t scale, uint64_t now_ms);

// For SETTINGS > GENERAL: what AUTO chose for the running game ("" = nothing).
void dvr_widget_note(const DvrWidget* widget, const Localization* strings, char* out, size_t out_size);

// Frees the thumbnails (the history goes with the game: core_session_stop).
void dvr_widget_destroy(DvrWidget* widget, AppShell* shell);
