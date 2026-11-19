#pragma once

// piccolo's IMAGE SETTINGS window, opened from the Picture tab of the video
// settings. The game's current frame on the left, at a whole-number scale (as
// big as fits next to the controls in piccolo's window); the image controls
// on the right, in collapsible sections: overscan crop, with the cut-off
// parts shaded on the picture, and pass 0's picture controls, shown on the
// kept part of the picture.
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

    // The picture after pass 0, the shown part only (after the crop); 0 =
    // none. Drawn over the kept part when it's the same size.
    uint64_t processed_texture_id = 0;
    uint32_t processed_width      = 0;
    uint32_t processed_height     = 0;

    bool controls_available = false;   // pass 0 runs (librashader loaded): else its rows show a note
};

// Draws the window while *open; the close button clears it. `source` may
// have no picture (texture_id 0): the controls still work.
void image_window_draw(bool* open, const SettingsUiContext* context, const ImageWindowSource* source,
                       ImFont* pixel_font, int32_t scale);
