#pragma once

// Button glyphs for on-screen hints ("[A] SELECT"), for the device used last:
// the gamepad's own buttons (Xbox, PlayStation, Nintendo...) or keyboard
// keycaps in the UI language.
//
// The pictures are assets/glyphs/ (32x32, white with a hard alpha mask, made
// by tools/make_glyphs.py; see its README), next to the program. Only the
// glyphs for the current device are loaded, side by side in one small
// texture, and they are reloaded when the device, the pad type or the
// language changes. Drawn tinted with the text color, nearest-filtered.
//
// Hints name an ACTION, not a button: confirm is the south face button on
// every pad (A on Xbox, B on Nintendo, cross on PlayStation, as SDL and
// ImGui do) and Return on the keyboard.

#include <cstdint>
#include <string>
#include <vector>

#include "vk_texture.h"

struct AppShell;
struct ImDrawList;
union SDL_Event;

enum class GlyphAction
{
    confirm,          // south / Return
    back,             // east / Escape
    move_vertical,    // d-pad up+down / the up and down keys
    move_horizontal,  // d-pad left+right / the left and right keys
    menu,             // start / Escape
    alternate,        // west / Space
    count,
};

enum class InputDeviceKind
{
    keyboard,   // also the mouse
    gamepad,
};

// One glyph's picture in the texture: GLYPH_SIZE x GLYPH_SIZE, at column `slot`.
constexpr uint32_t INPUT_GLYPH_SIZE = 32;
constexpr uint32_t INPUT_GLYPH_SLOTS = 8;   // at most two pictures per action, room to spare

struct InputGlyphs
{
    InputDeviceKind device = InputDeviceKind::keyboard;
    bool            any_input = false;    // a key or button was pressed (until then a connected pad wins)
    std::string     pad_folder = "xboxseries";   // assets/glyphs/<pad_folder> for the last gamepad
    std::string     language = "en";      // keyboard/<language>, then keyboard/en, then keyboard/common

    // What's loaded: for which device / pad / language, and where each
    // action's pictures are (-1 = none; the action has no glyph).
    bool        loaded = false;
    std::string loaded_key;
    int32_t     first_slot[static_cast<int>(GlyphAction::count)] = {};
    int32_t     slot_count[static_cast<int>(GlyphAction::count)] = {};

    std::vector<uint32_t> pixels;   // INPUT_GLYPH_SLOTS * SIZE wide, SIZE tall, B8G8R8A8
    StreamingTexture      texture;
    uint64_t              texture_id = 0;
    bool                  upload_pending = false;
    bool                  has_picture = false;
};

// Follows the device used last: a key or mouse button -> keyboard; a gamepad
// button or a stick pushed far -> that gamepad (and its type). Before any
// input, a gamepad being connected counts as using it. Call for every event.
void input_glyphs_handle_event(InputGlyphs* glyphs, const SDL_Event* event);

// The keyboard labels' language (piccolo's language codes: "en", "es").
void input_glyphs_set_language(InputGlyphs* glyphs, const char* language);

// Loads the glyphs for the current device when they changed. Call once a
// frame before drawing; cheap when nothing changed.
void input_glyphs_update(InputGlyphs* glyphs);

// Uploads newly loaded glyphs. Call after app_shell_acquire, outside rendering.
void input_glyphs_upload(InputGlyphs* glyphs, AppShell* shell);

// Frees the texture (waits for the GPU).
void input_glyphs_destroy(InputGlyphs* glyphs, AppShell* shell);

// The size (in window pixels) glyphs are drawn at next to text at `scale`:
// whole multiples of the 32-pixel pictures (16 = half, at scale 1-2), so
// they stay crisp, about as tall as a text line plus padding.
float input_glyphs_size(int32_t scale);

// Width of an action's glyphs at `size` (0 if it has none), with `gap`
// between two pictures.
float input_glyphs_width(const InputGlyphs* glyphs, GlyphAction action, float size, float gap);

// Draws an action's glyphs with their top-left at (x, y), tinted `color`
// (0xAARRGGBB). Returns the width drawn (as input_glyphs_width).
float input_glyphs_draw(const InputGlyphs* glyphs, ImDrawList* draw_list, GlyphAction action, float x, float y,
                        float size, float gap, uint32_t color);
