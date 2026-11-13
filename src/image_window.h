#pragma once

// piccolo's IMAGE SETTINGS window, opened from the Picture tab of the video
// settings. The game's current frame on the left, at a whole-number scale (as
// big as fits next to the controls in piccolo's window); the image controls
// on the right, in collapsible sections. For now: overscan crop, with the
// cut-off parts shaded on the picture. More image settings will join it.
//
//   +- IMAGE SETTINGS ------------------------------------------------+
//   | EDITING: THIS GAME                                               |
//   | +-------------------------------+  - OVERSCAN CROP               |
//   | |#####  cut off (shaded)  ######|  C CROP TOP    [- 8 +] [RESET] |
//   | |  the frame, x3, sharp pixels  |    CROP BOTTOM [- 8 +]         |
//   | |#############################  |    CROP LEFT   [- 0 +]         |
//   | +-------------------------------+    CROP RIGHT  [- 0 +]         |
//   +------------------------------------------------------------------+
//
// Edits go to the scope chosen in the settings window.

#include <cstdint>

#include "settings_ui.h"

struct ImFont;

// The game picture to show (the core session's texture).
struct ImageWindowSource
{
    uint64_t texture_id   = 0;      // ImGui texture id; 0 = no picture
    float    uv_max_x     = 0.0f;   // the used part of the texture
    float    uv_max_y     = 0.0f;
    uint32_t frame_width  = 0;
    uint32_t frame_height = 0;
};

// Draws the window while *open; the close button clears it. `source` may
// have no picture (texture_id 0): the controls still work.
void image_window_draw(bool* open, const SettingsUiContext* context, const ImageWindowSource* source,
                       ImFont* pixel_font, int32_t scale);
