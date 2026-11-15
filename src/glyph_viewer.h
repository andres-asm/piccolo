#pragma once

// TEMPORARY development window: every glyph PNG of one folder of
// assets/glyphs (a pad, or the keyboard in one language), with its file
// name, at a chosen zoom, nearest-filtered. For judging the glyphs from
// tools/make_glyphs.py on screen. F9, or from the debug window (MISC >
// DEBUG WINDOW). Remove when the glyphs are settled.

#include <cstdint>
#include <string>
#include <vector>

#include "vk_texture.h"

struct AppShell;

struct GlyphViewerPicture
{
    std::string name;
    uint32_t    x = 0;   // in the texture
    uint32_t    y = 0;
    uint32_t    width = 0;
    uint32_t    height = 0;
};

struct GlyphViewer
{
    bool    open = false;
    int32_t set = 0;      // index into the sizes (12, 16, 24, 32)
    int32_t folder = 0;   // index into the folders (pads, then keyboard)
    int32_t zoom = 4;
    bool    reload = true;

    std::vector<GlyphViewerPicture> pictures;
    std::vector<uint32_t>           pixels;   // the texture, B8G8R8A8
    StreamingTexture                texture;
    uint64_t                        texture_id = 0;
    bool                            upload_pending = false;
    bool                            has_picture = false;
};

// Draws the window while open (loads the folder when the choice changes).
void glyph_viewer_draw(GlyphViewer* viewer);

// Uploads newly loaded pictures. Call after app_shell_acquire, outside rendering.
void glyph_viewer_upload(GlyphViewer* viewer, AppShell* shell);

// Frees the texture (waits for the GPU).
void glyph_viewer_destroy(GlyphViewer* viewer, AppShell* shell);
