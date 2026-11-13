#include "pixel_font.h"

#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// The glyphs
// ---------------------------------------------------------------------------
//
// Written as pictures so anyone can read and edit them: '#' = pixel on,
// '.' = off. Drawn for piccolo (see pixel_font.h). Style: capitals that use
// the full 5x5 cell, rounded where a curve reads better, digits with a dot in
// the zero so 0 and O differ.

struct GlyphPicture
{
    uint32_t    codepoint;
    const char* rows[PIXEL_FONT_GLYPH_HEIGHT];
};

static const GlyphPicture GLYPHS[] = {
    { ' ', { ".....", ".....", ".....", ".....", "....." } },

    // Digits
    { '0', { ".###.", "#...#", "#.#.#", "#...#", ".###." } },
    { '1', { "..#..", ".##..", "..#..", "..#..", "#####" } },
    { '2', { ".###.", "#...#", "...#.", "..#..", "#####" } },
    { '3', { "####.", "....#", "..##.", "....#", "####." } },
    { '4', { "#..#.", "#..#.", "#####", "...#.", "...#." } },
    { '5', { "#####", "#....", "#####", "....#", "#####" } },
    { '6', { "..##.", ".#...", "####.", "#...#", ".###." } },
    { '7', { "#####", "....#", "...#.", "..#..", "..#.." } },
    { '8', { ".###.", "#...#", "#####", "#...#", ".###." } },
    { '9', { ".###.", "#...#", ".####", "...#.", ".##.." } },

    // Letters
    { 'A', { "..#..", ".#.#.", "#...#", "#####", "#...#" } },
    { 'B', { "####.", "#..#.", "####.", "#...#", "####." } },
    { 'C', { ".####", "#....", "#....", "#....", ".####" } },
    { 'D', { "###..", "#..#.", "#...#", "#..#.", "###.." } },
    { 'E', { "#####", "#....", "###..", "#....", "#####" } },
    { 'F', { "#####", "#....", "###..", "#....", "#...." } },
    { 'G', { ".###.", "#....", "#.###", "#...#", ".####" } },
    { 'H', { "#...#", "#...#", "#####", "#...#", "#...#" } },
    { 'I', { ".###.", "..#..", "..#..", "..#..", ".###." } },
    { 'J', { "....#", "....#", "....#", "#...#", ".###." } },
    { 'K', { "#...#", "#..#.", "###..", "#..#.", "#...#" } },
    { 'L', { "#....", "#....", "#....", "#....", "#####" } },
    { 'M', { "#...#", "##.##", "#.#.#", "#...#", "#...#" } },
    { 'N', { "#...#", "##..#", "#.#.#", "#..##", "#...#" } },
    { 'O', { ".###.", "#...#", "#...#", "#...#", ".###." } },
    { 'P', { "####.", "#...#", "#...#", "####.", "#...." } },
    { 'Q', { ".###.", "#...#", "#...#", ".###.", "...##" } },
    { 'R', { "####.", "#...#", "#...#", "####.", "#...#" } },
    { 'S', { ".###.", "#....", ".###.", "....#", ".###." } },
    { 'T', { "#####", "..#..", "..#..", "..#..", "..#.." } },
    { 'U', { "#...#", "#...#", "#...#", "#...#", ".###." } },
    { 'V', { "#...#", "#...#", "#...#", ".#.#.", "..#.." } },
    { 'W', { "#...#", "#...#", "#.#.#", "##.##", "#...#" } },
    { 'X', { "#...#", ".#.#.", "..#..", ".#.#.", "#...#" } },
    { 'Y', { "#...#", ".#.#.", "..#..", "..#..", "..#.." } },
    { 'Z', { "#####", "...#.", "..#..", ".#...", "#####" } },

    // Punctuation and symbols
    { '.', { ".....", ".....", ".....", ".....", "..#.." } },
    { ',', { ".....", ".....", ".....", "..#..", ".#..." } },
    { ':', { ".....", "..#..", ".....", "..#..", "....." } },
    { ';', { ".....", "..#..", ".....", "..#..", ".#..." } },
    { '!', { "..#..", "..#..", "..#..", ".....", "..#.." } },
    { '?', { "####.", "....#", "..##.", ".....", "..#.." } },
    { '-', { ".....", ".....", ".###.", ".....", "....." } },
    { '_', { ".....", ".....", ".....", ".....", "#####" } },
    { '+', { ".....", "..#..", ".###.", "..#..", "....." } },
    { '=', { ".....", ".###.", ".....", ".###.", "....." } },
    { '*', { ".....", "#.#.#", ".###.", "#.#.#", "....." } },
    { '/', { "....#", "...#.", "..#..", ".#...", "#...." } },
    { '\\', { "#....", ".#...", "..#..", "...#.", "....#" } },
    { '|', { "..#..", "..#..", "..#..", "..#..", "..#.." } },
    { '(', { "..#..", ".#...", ".#...", ".#...", "..#.." } },
    { ')', { "..#..", "...#.", "...#.", "...#.", "..#.." } },
    { '[', { ".##..", ".#...", ".#...", ".#...", ".##.." } },
    { ']', { "..##.", "...#.", "...#.", "...#.", "..##." } },
    { '{', { "..##.", "..#..", ".##..", "..#..", "..##." } },
    { '}', { ".##..", "..#..", "..##.", "..#..", ".##.." } },
    { '<', { "...#.", "..#..", ".#...", "..#..", "...#." } },
    { '>', { ".#...", "..#..", "...#.", "..#..", ".#..." } },
    { '\'', { "..#..", "..#..", ".....", ".....", "....." } },
    { '"', { "#.#..", "#.#..", ".....", ".....", "....." } },
    { '`', { ".#...", "..#..", ".....", ".....", "....." } },
    { '^', { "..#..", ".#.#.", ".....", ".....", "....." } },
    { '~', { ".....", ".#..#", "#.##.", ".....", "....." } },
    { '#', { ".#.#.", "#####", ".#.#.", "#####", ".#.#." } },
    { '$', { ".###.", "#.#..", ".###.", "..#.#", ".###." } },
    { '%', { "##..#", "##.#.", "..#..", ".#.##", "#..##" } },
    { '&', { ".##..", "#..#.", ".##.#", "#..#.", ".##.#" } },
    { '@', { ".###.", "#.#.#", "#.###", "#....", ".####" } },

    // Latin-1 letters and symbols that aren't "letter + accent"
    { 0x00C6, { ".####", "#.#..", "#####", "#.#..", "#.###" } },   // Æ
    { 0x00D0, { "###..", "#..#.", "##..#", "#..#.", "###.." } },   // Ð
    { 0x00D8, { ".###.", "#..##", "#.#.#", "##..#", ".###." } },   // Ø
    { 0x00DE, { "#....", "####.", "#...#", "####.", "#...." } },   // Þ
    { 0x00DF, { ".##..", "#..#.", "#.#..", "#..#.", "#.##." } },   // ß
    { 0x0152, { ".####", "#.#..", "#.###", "#.#..", ".####" } },   // Œ
    { 0x00A1, { "..#..", ".....", "..#..", "..#..", "..#.." } },   // ¡
    { 0x00BF, { "..#..", ".....", ".##..", "#....", ".####" } },   // ¿
    { 0x00AB, { ".....", "..#.#", ".#.#.", "..#.#", "....." } },   // «
    { 0x00BB, { ".....", "#.#..", ".#.#.", "#.#..", "....." } },   // »
    { 0x00B0, { ".#...", "#.#..", ".#...", ".....", "....." } },   // °
    { 0x00B7, { ".....", ".....", "..#..", ".....", "....." } },   // ·
    { 0x00A3, { "..##.", ".#..#", "###..", ".#...", "#####" } },   // £
    { 0x00A2, { "..#..", ".####", "#.#..", ".####", "..#.." } },   // ¢
    { 0x00A5, { "#...#", ".#.#.", "#####", "..#..", "..#.." } },   // ¥
    { 0x00B1, { "..#..", ".###.", "..#..", ".....", ".###." } },   // ±
    { 0x00D7, { ".....", ".#.#.", "..#..", ".#.#.", "....." } },   // ×
    { 0x00F7, { "..#..", ".....", "#####", ".....", "..#.." } },   // ÷
    { 0x20AC, { "..###", ".#...", "####.", ".#...", "..###" } },   // €
    { 0x2026, { ".....", ".....", ".....", ".....", "#.#.#" } },   // …
    { 0x2022, { ".....", ".###.", ".###.", ".###.", "....." } },   // •

    // UI symbols
    { PIXEL_FONT_ARROW_UP,    { ".....", "..#..", ".###.", "#####", "....." } },
    { PIXEL_FONT_ARROW_DOWN,  { ".....", "#####", ".###.", "..#..", "....." } },
    { PIXEL_FONT_ARROW_LEFT,  { "...#.", "..##.", ".###.", "..##.", "...#." } },
    { PIXEL_FONT_ARROW_RIGHT, { ".#...", ".##..", ".###.", ".##..", ".#..." } },
    { PIXEL_FONT_CHECK,       { "....#", "...##", "#.##.", "###..", ".#..." } },
    { PIXEL_FONT_MENU,        { "#####", ".....", "#####", ".....", "#####" } },
};

constexpr uint32_t PICTURE_COUNT = sizeof(GLYPHS) / sizeof(GLYPHS[0]);

// Accents: drawn in the 2 rows above the letter, or the 1 row below it.
struct Mark
{
    const char* above[PIXEL_FONT_MARK_ROWS];   // nullptr = nothing above
    const char* below;                          // nullptr = nothing below
};

static const Mark ACUTE      = { { "...#.", "..#.." }, nullptr };
static const Mark GRAVE      = { { ".#...", "..#.." }, nullptr };
static const Mark CIRCUMFLEX = { { "..#..", ".#.#." }, nullptr };
static const Mark CARON      = { { ".#.#.", "..#.." }, nullptr };
static const Mark DIAERESIS  = { { ".....", ".#.#." }, nullptr };
static const Mark TILDE      = { { ".#..#", "#.##." }, nullptr };
static const Mark RING       = { { ".###.", ".#.#." }, nullptr };
static const Mark CEDILLA    = { { nullptr, nullptr }, "..#.." };

// Accented capitals: a letter from the table above plus a mark.
struct ComposedGlyph
{
    uint32_t    codepoint;
    uint32_t    base;
    const Mark* mark;
};

static const ComposedGlyph COMPOSED[] = {
    { 0x00C0, 'A', &GRAVE },     { 0x00C1, 'A', &ACUTE },   { 0x00C2, 'A', &CIRCUMFLEX },
    { 0x00C3, 'A', &TILDE },     { 0x00C4, 'A', &DIAERESIS }, { 0x00C5, 'A', &RING },
    { 0x00C7, 'C', &CEDILLA },
    { 0x00C8, 'E', &GRAVE },     { 0x00C9, 'E', &ACUTE },   { 0x00CA, 'E', &CIRCUMFLEX }, { 0x00CB, 'E', &DIAERESIS },
    { 0x00CC, 'I', &GRAVE },     { 0x00CD, 'I', &ACUTE },   { 0x00CE, 'I', &CIRCUMFLEX }, { 0x00CF, 'I', &DIAERESIS },
    { 0x00D1, 'N', &TILDE },
    { 0x00D2, 'O', &GRAVE },     { 0x00D3, 'O', &ACUTE },   { 0x00D4, 'O', &CIRCUMFLEX },
    { 0x00D5, 'O', &TILDE },     { 0x00D6, 'O', &DIAERESIS },
    { 0x00D9, 'U', &GRAVE },     { 0x00DA, 'U', &ACUTE },   { 0x00DB, 'U', &CIRCUMFLEX }, { 0x00DC, 'U', &DIAERESIS },
    { 0x00DD, 'Y', &ACUTE },
    { 0x0160, 'S', &CARON },     { 0x017D, 'Z', &CARON },   { 0x0178, 'Y', &DIAERESIS },
};

constexpr uint32_t COMPOSED_COUNT = sizeof(COMPOSED) / sizeof(COMPOSED[0]);

// Characters drawn with another one's glyph.
struct Alias
{
    uint32_t codepoint;
    uint32_t same_as;
};

static const Alias ALIASES[] = {
    { 0x00A0, ' ' },    // no-break space
    { 0x2018, '\'' },   // ‘
    { 0x2019, '\'' },   // ’
    { 0x201C, '"' },    // “
    { 0x201D, '"' },    // ”
    { 0x2013, '-' },    // –
    { 0x2014, '-' },    // —
    { 0x0131, 'I' },    // dotless i
};

constexpr uint32_t ALIAS_COUNT = sizeof(ALIASES) / sizeof(ALIASES[0]);

// Drawn for characters without a glyph.
static const char* const MISSING_ROWS[PIXEL_FONT_GLYPH_HEIGHT] = { "#####", "#...#", "#...#", "#...#", "#####" };

// ---------------------------------------------------------------------------
// Lookup table built from the pictures
// ---------------------------------------------------------------------------

constexpr uint32_t MAX_GLYPHS = PICTURE_COUNT + COMPOSED_COUNT;
constexpr uint32_t DIRECT_LOOKUP = 0x180;   // ASCII, Latin-1, Latin Extended-A: by index

struct BuiltGlyph
{
    uint32_t codepoint;
    uint8_t  rows[PIXEL_FONT_CELL_HEIGHT];
};

static BuiltGlyph g_glyphs[MAX_GLYPHS] = {};
static uint32_t   g_glyph_count = 0;
static int32_t    g_direct[DIRECT_LOOKUP] = {};   // index into g_glyphs, -1 = none
static uint8_t    g_missing[PIXEL_FONT_CELL_HEIGHT] = {};
static bool       g_ready = false;

static bool row_to_bits(const char* row, uint8_t* bits)
{
    if (row == nullptr || std::strlen(row) != static_cast<size_t>(PIXEL_FONT_GLYPH_WIDTH))
    {
        return false;
    }
    uint8_t value = 0;
    for (int32_t x = 0; x < PIXEL_FONT_GLYPH_WIDTH; x++)
    {
        value = static_cast<uint8_t>(value << 1);
        if (row[x] == '#')
        {
            value = static_cast<uint8_t>(value | 1u);
        }
        else if (row[x] != '.')
        {
            return false;
        }
    }
    *bits = value;
    return true;
}

// Index of the glyph drawn for `codepoint` (its own, an alias's, or -1).
static int32_t find_glyph(uint32_t codepoint)
{
    for (uint32_t i = 0; i < ALIAS_COUNT; i++)
    {
        if (ALIASES[i].codepoint == codepoint)
        {
            codepoint = ALIASES[i].same_as;
            break;
        }
    }
    if (codepoint < DIRECT_LOOKUP)
    {
        return g_direct[codepoint];
    }
    for (uint32_t i = 0; i < g_glyph_count; i++)
    {
        if (g_glyphs[i].codepoint == codepoint)
        {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

static bool add_glyph(uint32_t codepoint, BuiltGlyph** glyph)
{
    if (find_glyph(codepoint) >= 0)
    {
        std::fprintf(stderr, "pixel_font: U+%04X is defined twice\n", codepoint);
        return false;
    }
    *glyph = &g_glyphs[g_glyph_count];
    **glyph = BuiltGlyph{};
    (*glyph)->codepoint = codepoint;
    if (codepoint < DIRECT_LOOKUP)
    {
        g_direct[codepoint] = static_cast<int32_t>(g_glyph_count);
    }
    g_glyph_count++;
    return true;
}

bool pixel_font_init()
{
    g_ready = false;
    g_glyph_count = 0;
    for (uint32_t i = 0; i < DIRECT_LOOKUP; i++)
    {
        g_direct[i] = -1;
    }
    std::memset(g_missing, 0, sizeof(g_missing));
    for (int32_t y = 0; y < PIXEL_FONT_GLYPH_HEIGHT; y++)
    {
        row_to_bits(MISSING_ROWS[y], &g_missing[PIXEL_FONT_MARK_ROWS + y]);
    }

    for (uint32_t i = 0; i < PICTURE_COUNT; i++)
    {
        const GlyphPicture* picture = &GLYPHS[i];
        BuiltGlyph* glyph = nullptr;
        if (!add_glyph(picture->codepoint, &glyph))
        {
            return false;
        }
        for (int32_t y = 0; y < PIXEL_FONT_GLYPH_HEIGHT; y++)
        {
            if (!row_to_bits(picture->rows[y], &glyph->rows[PIXEL_FONT_MARK_ROWS + y]))
            {
                std::fprintf(stderr, "pixel_font: U+%04X row %d is not 5 of '#'/'.'\n", picture->codepoint, y);
                return false;
            }
        }
    }

    // Accented letters: the base letter's rows plus the mark's.
    for (uint32_t i = 0; i < COMPOSED_COUNT; i++)
    {
        const ComposedGlyph* composed = &COMPOSED[i];
        int32_t base = find_glyph(composed->base);
        if (base < 0)
        {
            std::fprintf(stderr, "pixel_font: U+%04X is built on U+%04X, which has no glyph\n", composed->codepoint,
                         composed->base);
            return false;
        }
        uint8_t base_rows[PIXEL_FONT_CELL_HEIGHT] = {};
        std::memcpy(base_rows, g_glyphs[base].rows, sizeof(base_rows));
        BuiltGlyph* glyph = nullptr;
        if (!add_glyph(composed->codepoint, &glyph))
        {
            return false;
        }
        std::memcpy(glyph->rows, base_rows, sizeof(base_rows));
        bool ok = true;
        for (int32_t y = 0; y < PIXEL_FONT_MARK_ROWS; y++)
        {
            if (composed->mark->above[y] != nullptr)
            {
                ok = ok && row_to_bits(composed->mark->above[y], &glyph->rows[y]);
            }
        }
        if (composed->mark->below != nullptr)
        {
            ok = ok && row_to_bits(composed->mark->below, &glyph->rows[PIXEL_FONT_CELL_HEIGHT - 1]);
        }
        if (!ok)
        {
            std::fprintf(stderr, "pixel_font: a mark used by U+%04X is not 5 of '#'/'.'\n", composed->codepoint);
            return false;
        }
    }
    g_ready = true;
    return true;
}

// Lowercase draws as uppercase: the font is capitals only.
static uint32_t to_capital(uint32_t codepoint)
{
    if (codepoint >= 'a' && codepoint <= 'z')
    {
        return codepoint - 'a' + 'A';
    }
    if (codepoint >= 0xE0 && codepoint <= 0xFE && codepoint != 0xF7)   // à..þ (not ÷)
    {
        return codepoint - 0x20;
    }
    switch (codepoint)
    {
    case 0x00FF:
        return 0x0178;   // ÿ -> Ÿ
    case 0x0153:
        return 0x0152;   // œ -> Œ
    case 0x0161:
        return 0x0160;   // š -> Š
    case 0x017E:
        return 0x017D;   // ž -> Ž
    default:
        return codepoint;
    }
}

bool pixel_font_glyph_rows(uint32_t codepoint, uint8_t rows[PIXEL_FONT_CELL_HEIGHT])
{
    int32_t index = g_ready ? find_glyph(to_capital(codepoint)) : -1;
    const uint8_t* source = index >= 0 ? g_glyphs[index].rows : g_missing;
    std::memcpy(rows, source, PIXEL_FONT_CELL_HEIGHT);
    return index >= 0;
}

uint32_t pixel_font_glyph_count()
{
    return g_glyph_count;
}

uint32_t pixel_font_glyph_codepoint(uint32_t index)
{
    return index < g_glyph_count ? g_glyphs[index].codepoint : 0;
}

uint32_t pixel_font_next_codepoint(const char** text)
{
    const unsigned char* bytes = static_cast<const unsigned char*>(static_cast<const void*>(*text));
    uint32_t first = bytes[0];
    if (first == 0)
    {
        return 0;
    }
    // Length from the first byte; continuation bytes are 10xxxxxx.
    uint32_t length = first < 0x80 ? 1u : (first >> 5) == 0x6u ? 2u : (first >> 4) == 0xEu ? 3u : (first >> 3) == 0x1Eu ? 4u : 0u;
    if (length == 0)
    {
        *text += 1;
        return 0xFFFD;
    }
    uint32_t codepoint = length == 1 ? first : first & (0xFFu >> (length + 1));
    for (uint32_t i = 1; i < length; i++)
    {
        if ((bytes[i] & 0xC0u) != 0x80u)
        {
            *text += i;   // cut short: resume at the byte that broke it
            return 0xFFFD;
        }
        codepoint = (codepoint << 6) | (bytes[i] & 0x3Fu);
    }
    *text += length;
    return codepoint;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

static void fill_block(uint32_t* pixels, int32_t width, int32_t height, int32_t pitch_pixels,
                       int32_t x, int32_t y, int32_t size, uint32_t color)
{
    for (int32_t block_y = y; block_y < y + size; block_y++)
    {
        if (block_y < 0 || block_y >= height)
        {
            continue;
        }
        for (int32_t block_x = x; block_x < x + size; block_x++)
        {
            if (block_x >= 0 && block_x < width)
            {
                pixels[static_cast<size_t>(block_y) * static_cast<size_t>(pitch_pixels) + static_cast<size_t>(block_x)] = color;
            }
        }
    }
}

void pixel_font_draw_text(uint32_t* pixels, int32_t width, int32_t height, int32_t pitch_pixels,
                          int32_t x, int32_t y, const char* text, uint32_t color, int32_t scale)
{
    int32_t pen_x = x;
    int32_t pen_y = y;
    const char* cursor = text;
    uint32_t codepoint = pixel_font_next_codepoint(&cursor);
    while (codepoint != 0)
    {
        if (codepoint == '\n')
        {
            pen_x = x;
            pen_y += PIXEL_FONT_LINE_HEIGHT * scale;
            codepoint = pixel_font_next_codepoint(&cursor);
            continue;
        }

        uint8_t rows[PIXEL_FONT_CELL_HEIGHT] = {};
        pixel_font_glyph_rows(codepoint, rows);
        for (int32_t row = 0; row < PIXEL_FONT_CELL_HEIGHT; row++)
        {
            for (int32_t column = 0; column < PIXEL_FONT_GLYPH_WIDTH; column++)
            {
                // Bit 4 is the leftmost pixel.
                if ((static_cast<uint32_t>(rows[row]) >> static_cast<uint32_t>(PIXEL_FONT_GLYPH_WIDTH - 1 - column)) & 1u)
                {
                    fill_block(pixels, width, height, pitch_pixels, pen_x + column * scale, pen_y + row * scale, scale, color);
                }
            }
        }
        pen_x += PIXEL_FONT_ADVANCE * scale;
        codepoint = pixel_font_next_codepoint(&cursor);
    }
}

void pixel_font_measure(const char* text, int32_t scale, int32_t* width, int32_t* height)
{
    int32_t longest = 0;
    int32_t current = 0;
    int32_t lines = 1;
    const char* cursor = text;
    uint32_t codepoint = pixel_font_next_codepoint(&cursor);
    while (codepoint != 0)
    {
        if (codepoint == '\n')
        {
            lines++;
            current = 0;
        }
        else
        {
            current++;
            longest = current > longest ? current : longest;
        }
        codepoint = pixel_font_next_codepoint(&cursor);
    }
    // The last character needs no gap after it.
    *width = longest > 0 ? (longest * PIXEL_FONT_ADVANCE - 1) * scale : 0;
    *height = lines * PIXEL_FONT_LINE_HEIGHT * scale;
}
