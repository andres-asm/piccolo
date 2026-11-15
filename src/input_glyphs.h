#pragma once

// Button glyphs for on-screen hints ("[A] SELECT"), for the device used last:
// the gamepad's own buttons (Xbox, PlayStation, Nintendo...) or keyboard
// keycaps in the UI language.
//
// The pictures are PNG files in assets/glyphs/12/ next to the program
// (white with a hard alpha mask; made by tools/make_glyphs.py, see its
// README): 12 pixels tall, drawn pixel by pixel with piccolo's own 5x5 font,
// 12 x N wide when a label needs it (START is 36, SELECT 48). Each picture is
// drawn at the whole multiple of ITS height closest to a text line (8 retro
// pixels) without going over: x2 at 1080p, so 24 pixels tall. Width and
// height come from the file, so a theme can replace any of them (other
// sizes too) without code. Only the glyphs for the current device are
// loaded, side by side in one small texture, and they are reloaded when the
// device, the pad type or the language changes. Drawn tinted with the text
// color, nearest-filtered.
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

// The texture the loaded pictures are packed into, left to right.
constexpr uint32_t INPUT_GLYPH_TEXTURE_WIDTH = 512;
constexpr uint32_t INPUT_GLYPH_TEXTURE_HEIGHT = 64;   // the tallest picture allowed
constexpr int32_t  INPUT_GLYPH_PICTURES = 12;          // at most two per action

struct InputGlyphPicture
{
    uint32_t x = 0;        // where it is in the texture
    uint32_t width = 0;    // its size in the file
    uint32_t height = 0;
};

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
    InputGlyphPicture pictures[INPUT_GLYPH_PICTURES] = {};
    int32_t     first_slot[static_cast<int>(GlyphAction::count)] = {};   // into pictures
    int32_t     slot_count[static_cast<int>(GlyphAction::count)] = {};
    int32_t     line_height = 24;     // a text line in window pixels (8 retro pixels), from the scale

    std::vector<uint32_t> pixels;   // the texture's pixels, B8G8R8A8
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

// Loads the glyphs for the current device when they changed, and sets the
// size they're drawn at from the UI scale `scale` (window pixels per retro
// pixel). Call once a frame before drawing; cheap when nothing changed.
void input_glyphs_update(InputGlyphs* glyphs, int32_t scale);

// Uploads newly loaded glyphs. Call after app_shell_acquire, outside rendering.
void input_glyphs_upload(InputGlyphs* glyphs, AppShell* shell);

// Frees the texture (waits for the GPU).
void input_glyphs_destroy(InputGlyphs* glyphs, AppShell* shell);

// The height (window pixels) of the tallest loaded glyph as drawn: at most
// a text line (8 retro pixels at the scale given to input_glyphs_update()).
float input_glyphs_size(const InputGlyphs* glyphs);

// Width of an action's glyphs as drawn (0 if it has none), with `gap`
// between two pictures.
float input_glyphs_width(const InputGlyphs* glyphs, GlyphAction action, float gap);

// Draws an action's glyphs from x, each centered vertically in a band
// `height` tall starting at y, tinted `color` (0xAARRGGBB). Returns the
// width drawn (as input_glyphs_width).
float input_glyphs_draw(const InputGlyphs* glyphs, ImDrawList* draw_list, GlyphAction action, float x, float y,
                        float height, float gap, uint32_t color);
