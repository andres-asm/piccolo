#pragma once

// Button glyphs for on-screen hints ("[A] SELECT"), for the device used last:
// the gamepad's own buttons (Xbox, PlayStation, Nintendo...) or keyboard
// keycaps in the UI language.
//
// The pictures are assets/glyphs/<size>/ (white with a hard alpha mask, made
// by tools/make_glyphs.py; see its README), next to the program, drawn
// natively at 12, 16, 24 and 32 pixels. Glyphs are a text line tall: the set
// for that size, or a whole multiple of one (4K: the 24 set doubled), never
// a shrunk one (pixels would drop out). Only the glyphs for the current
// device are loaded, side by side in one small texture, and they are
// reloaded when the device, the pad type, the language or the size changes.
// Drawn tinted with the text color, nearest-filtered.
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

// The texture: INPUT_GLYPH_SLOTS cells of INPUT_GLYPH_CELL pixels, side by
// side; a glyph of the loaded size sits in the top-left of its cell.
constexpr uint32_t INPUT_GLYPH_CELL = 32;   // the largest set
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
    uint32_t    native_size = 32;     // the set loaded (12, 16, 24, 32)
    float       draw_size = 32.0f;    // drawn at native_size x a whole number
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

// Loads the glyphs for the current device, at the size for UI scale `scale`
// (window pixels per retro pixel), when they changed. Call once a frame
// before drawing; cheap when nothing changed.
void input_glyphs_update(InputGlyphs* glyphs, int32_t scale);

// Uploads newly loaded glyphs. Call after app_shell_acquire, outside rendering.
void input_glyphs_upload(InputGlyphs* glyphs, AppShell* shell);

// Frees the texture (waits for the GPU).
void input_glyphs_destroy(InputGlyphs* glyphs, AppShell* shell);

// The size (in window pixels) the loaded glyphs are drawn at: about a text
// line (8 retro pixels) at the scale given to input_glyphs_update().
float input_glyphs_size(const InputGlyphs* glyphs);

// Width of an action's glyphs at `size` (0 if it has none), with `gap`
// between two pictures.
float input_glyphs_width(const InputGlyphs* glyphs, GlyphAction action, float size, float gap);

// Draws an action's glyphs with their top-left at (x, y), tinted `color`
// (0xAARRGGBB). Returns the width drawn (as input_glyphs_width).
float input_glyphs_draw(const InputGlyphs* glyphs, ImDrawList* draw_list, GlyphAction action, float x, float y,
                        float size, float gap, uint32_t color);
