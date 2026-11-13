#pragma once

// piccolo's pixel font: 5x5 capitals for the retro-style UI.
//
// Drawn for piccolo from scratch: every glyph was designed as "a 5x5 letter",
// not traced from any other font. (The retro UI recreates the FEEL of a classic
// emulator GUI, but that software is GPL: its font, logo and graphics are never
// copied. See CLAUDE.md.)
//
// Layout: each letter is 5x5 pixels inside an 8-pixel-tall cell: 2 rows above
// it for accents, 1 below for cedillas and the like. Text advances 6 pixels
// per character (1 pixel gap) and 8 per line. Characters are Unicode code
// points (text is UTF-8): ASCII, Latin-1 (accented letters for Western
// European languages, ¡ ¿ « » € ...), a few from Latin Extended-A (Œ Š Ž Ÿ)
// and the UI symbols below. Lowercase letters draw as capitals. Characters
// without a glyph draw as a hollow box, so missing ones are obvious.

#include <cstdint>

constexpr int32_t PIXEL_FONT_GLYPH_WIDTH  = 5;
constexpr int32_t PIXEL_FONT_GLYPH_HEIGHT = 5;   // the letter itself
constexpr int32_t PIXEL_FONT_MARK_ROWS    = 2;   // above the letter, for accents
constexpr int32_t PIXEL_FONT_CELL_HEIGHT  = 8;   // marks + letter + 1 row below
constexpr int32_t PIXEL_FONT_ADVANCE      = 6;   // glyph width + 1 pixel gap
constexpr int32_t PIXEL_FONT_LINE_HEIGHT  = 8;   // = the cell

// UI symbols, by their Unicode code points.
constexpr uint32_t PIXEL_FONT_ARROW_UP    = 0x25B2;   // BLACK UP-POINTING TRIANGLE
constexpr uint32_t PIXEL_FONT_ARROW_DOWN  = 0x25BC;   // BLACK DOWN-POINTING TRIANGLE
constexpr uint32_t PIXEL_FONT_ARROW_LEFT  = 0x25C0;   // BLACK LEFT-POINTING TRIANGLE
constexpr uint32_t PIXEL_FONT_ARROW_RIGHT = 0x25B6;   // BLACK RIGHT-POINTING TRIANGLE
constexpr uint32_t PIXEL_FONT_CHECK       = 0x2714;   // HEAVY CHECK MARK
constexpr uint32_t PIXEL_FONT_MENU        = 0x2630;   // TRIGRAM FOR HEAVEN (the "hamburger" menu)

// One cell row as 5 bits: bit 4 = leftmost pixel, bit 0 = rightmost. Row 0 is
// the top of the cell (accents), rows 2..6 the letter, row 7 below it.
// Returns false if `codepoint` has no glyph (the hollow box is returned then).
// Call pixel_font_init() once first.
bool pixel_font_glyph_rows(uint32_t codepoint, uint8_t rows[PIXEL_FONT_CELL_HEIGHT]);

// Builds the bit rows from the readable tables in pixel_font.cpp, checking
// them (rows of exactly 5 '#'/'.' characters, nothing defined twice, accented
// letters built on letters that exist). Returns false and logs the first
// problem if a table is malformed.
bool pixel_font_init();

// Decodes the next UTF-8 character of *text and moves past it. Invalid bytes
// come back as U+FFFD (no glyph: drawn as the box). 0 at the end.
uint32_t pixel_font_next_codepoint(const char** text);

// Draws UTF-8 `text` into a 32-bit pixel buffer (pitch in pixels), clipped to
// it. (x, y) is the top of the first line's cell; each font pixel becomes a
// scale x scale block. '\n' starts a new line.
void pixel_font_draw_text(uint32_t* pixels, int32_t width, int32_t height, int32_t pitch_pixels,
                          int32_t x, int32_t y, const char* text, uint32_t color, int32_t scale);

// Width and height in pixels `text` takes at `scale` (longest line, all lines).
void pixel_font_measure(const char* text, int32_t scale, int32_t* width, int32_t* height);

// Every code point that has its own glyph, in table order (previews, tests).
uint32_t pixel_font_glyph_count();
uint32_t pixel_font_glyph_codepoint(uint32_t index);
