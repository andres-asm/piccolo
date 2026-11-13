// pixel_font_preview: draws piccolo's pixel font into a PNG so the glyphs can
// be judged by eye, and checks the font table.
//
// Usage:
//   pixel_font_preview [--scale N] [output.png]   default: pixel_font_preview.png, scale 4
//   pixel_font_preview --check                    table checks only (CTest)
//
// The PNG is written by hand (uncompressed deflate "stored" blocks), so the
// tool needs no image library.

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pixel_font.h"

// Retro-ish colors (0xAARRGGBB).
static constexpr uint32_t BACKGROUND = 0xFF20304Cu;
static constexpr uint32_t TEXT_COLOR = 0xFFE8E8F0u;
static constexpr uint32_t DIM_COLOR  = 0xFF8090B0u;
static constexpr uint32_t CELL_COLOR = 0xFF283A5Cu;

// ---------------------------------------------------------------------------
// Minimal PNG writer
// ---------------------------------------------------------------------------

static uint32_t g_crc_table[256] = {};

static void build_crc_table()
{
    for (uint32_t n = 0; n < 256; n++)
    {
        uint32_t c = n;
        for (int k = 0; k < 8; k++)
        {
            c = (c & 1u) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        }
        g_crc_table[n] = c;
    }
}

static uint32_t crc_update(uint32_t crc, const uint8_t* data, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        crc = g_crc_table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc;
}

static void write_u32_be(FILE* file, uint32_t value)
{
    uint8_t bytes[4] = {
        static_cast<uint8_t>(value >> 24), static_cast<uint8_t>(value >> 16),
        static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value),
    };
    std::fwrite(bytes, 1, 4, file);
}

static void write_chunk(FILE* file, const char type[4], const uint8_t* data, size_t size)
{
    write_u32_be(file, static_cast<uint32_t>(size));
    std::fwrite(type, 1, 4, file);
    if (size > 0)
    {
        std::fwrite(data, 1, size, file);
    }
    uint32_t crc = crc_update(0xFFFFFFFFu, reinterpret_cast<const uint8_t*>(type), 4);
    crc = crc_update(crc, data, size);
    write_u32_be(file, crc ^ 0xFFFFFFFFu);
}

// pixels: width*height 0xAARRGGBB values. Written as 8-bit RGB.
static bool write_png(const char* path, const uint32_t* pixels, int32_t width, int32_t height)
{
    // Raw image data: each row starts with filter byte 0, then R,G,B per pixel.
    size_t row_size = 1 + static_cast<size_t>(width) * 3;
    size_t raw_size = row_size * static_cast<size_t>(height);
    uint8_t* raw = new uint8_t[raw_size];
    for (int32_t y = 0; y < height; y++)
    {
        uint8_t* row = raw + static_cast<size_t>(y) * row_size;
        row[0] = 0;
        for (int32_t x = 0; x < width; x++)
        {
            uint32_t pixel = pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)];
            row[1 + x * 3 + 0] = static_cast<uint8_t>(pixel >> 16);
            row[1 + x * 3 + 1] = static_cast<uint8_t>(pixel >> 8);
            row[1 + x * 3 + 2] = static_cast<uint8_t>(pixel);
        }
    }

    // zlib stream with stored (uncompressed) deflate blocks of at most 65535 bytes.
    size_t block_count = (raw_size + 65534) / 65535;
    size_t zlib_size = 2 + raw_size + block_count * 5 + 4;
    uint8_t* zlib = new uint8_t[zlib_size];
    size_t out = 0;
    zlib[out++] = 0x78;   // deflate, 32 KB window
    zlib[out++] = 0x01;   // no preset dictionary, fastest; (0x7801 % 31 == 0)
    uint32_t adler_a = 1;
    uint32_t adler_b = 0;
    for (size_t offset = 0; offset < raw_size; offset += 65535)
    {
        size_t length = raw_size - offset < 65535 ? raw_size - offset : 65535;
        bool last = offset + length == raw_size;
        zlib[out++] = last ? 1 : 0;
        zlib[out++] = static_cast<uint8_t>(length);
        zlib[out++] = static_cast<uint8_t>(length >> 8);
        zlib[out++] = static_cast<uint8_t>(~length);
        zlib[out++] = static_cast<uint8_t>(~length >> 8);
        std::memcpy(zlib + out, raw + offset, length);
        out += length;
        for (size_t i = 0; i < length; i++)
        {
            adler_a = (adler_a + raw[offset + i]) % 65521u;
            adler_b = (adler_b + adler_a) % 65521u;
        }
    }
    uint32_t adler = (adler_b << 16) | adler_a;
    zlib[out++] = static_cast<uint8_t>(adler >> 24);
    zlib[out++] = static_cast<uint8_t>(adler >> 16);
    zlib[out++] = static_cast<uint8_t>(adler >> 8);
    zlib[out++] = static_cast<uint8_t>(adler);

    bool ok = false;
    FILE* file = std::fopen(path, "wb");
    if (file != nullptr)
    {
        static const uint8_t SIGNATURE[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
        std::fwrite(SIGNATURE, 1, 8, file);

        uint8_t header[13] = {};
        header[0] = static_cast<uint8_t>(width >> 24);
        header[1] = static_cast<uint8_t>(width >> 16);
        header[2] = static_cast<uint8_t>(width >> 8);
        header[3] = static_cast<uint8_t>(width);
        header[4] = static_cast<uint8_t>(height >> 24);
        header[5] = static_cast<uint8_t>(height >> 16);
        header[6] = static_cast<uint8_t>(height >> 8);
        header[7] = static_cast<uint8_t>(height);
        header[8] = 8;    // bits per channel
        header[9] = 2;    // RGB
        write_chunk(file, "IHDR", header, sizeof(header));
        write_chunk(file, "IDAT", zlib, out);
        write_chunk(file, "IEND", nullptr, 0);
        ok = std::fclose(file) == 0;
    }

    delete[] zlib;
    delete[] raw;
    return ok;
}

// ---------------------------------------------------------------------------
// Checks
// ---------------------------------------------------------------------------

// No two characters may look the same (e.g. 0 and O): that's unreadable in a UI.
static bool check_glyphs_distinct()
{
    uint32_t count = pixel_font_glyph_count();
    bool ok = true;
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t first = pixel_font_glyph_codepoint(i);
        uint8_t a[PIXEL_FONT_CELL_HEIGHT] = {};
        pixel_font_glyph_rows(first, a);
        for (uint32_t j = i + 1; j < count; j++)
        {
            uint32_t second = pixel_font_glyph_codepoint(j);
            uint8_t b[PIXEL_FONT_CELL_HEIGHT] = {};
            pixel_font_glyph_rows(second, b);
            if (std::memcmp(a, b, sizeof(a)) == 0 && first != ' ')
            {
                std::printf("FAIL glyphs U+%04X and U+%04X are identical\n", first, second);
                ok = false;
            }
        }
    }
    return ok;
}

// Lowercase and the other spellings must find a glyph (not the box).
static bool check_lookups()
{
    static const uint32_t MUST_EXIST[] = {
        'a', 'z', 0x00E1, 0x00F1, 0x00FC, 0x00E7, 0x00E3, 0x00F5, 0x00DF, 0x00BF, 0x00A1, 0x00FF, 0x0153,
        0x0161, 0x017E, 0x20AC, 0x00A0, 0x2019, PIXEL_FONT_ARROW_DOWN, PIXEL_FONT_CHECK,
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof(MUST_EXIST) / sizeof(MUST_EXIST[0]); i++)
    {
        uint8_t rows[PIXEL_FONT_CELL_HEIGHT] = {};
        if (!pixel_font_glyph_rows(MUST_EXIST[i], rows))
        {
            std::printf("FAIL U+%04X has no glyph\n", MUST_EXIST[i]);
            ok = false;
        }
    }
    const char* text = "\xC3\xB1\xE2\x82\xAC\xFF!";   // ñ € (an invalid byte) !
    uint32_t expected[] = {0x00F1, 0x20AC, 0xFFFD, '!', 0};
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
    {
        uint32_t got = pixel_font_next_codepoint(&text);
        if (got != expected[i])
        {
            std::printf("FAIL UTF-8 decode %zu: expected U+%04X, got U+%04X\n", i, expected[i], got);
            ok = false;
        }
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Preview image
// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
    const char* output = "pixel_font_preview.png";
    int32_t scale = 4;
    bool check_only = false;

    for (int i = 1; i < argc; i++)
    {
        if (std::strcmp(argv[i], "--check") == 0)
        {
            check_only = true;
        }
        else if (std::strcmp(argv[i], "--scale") == 0 && i + 1 < argc)
        {
            i++;
            scale = static_cast<int32_t>(std::strtol(argv[i], nullptr, 10));
            if (scale < 1 || scale > 16)
            {
                std::fprintf(stderr, "--scale must be 1..16\n");
                return 2;
            }
        }
        else
        {
            output = argv[i];
        }
    }

    if (!pixel_font_init() || !check_glyphs_distinct() || !check_lookups())
    {
        return 1;
    }
    std::printf("pixel font: %u glyphs, table OK\n", pixel_font_glyph_count());
    if (check_only)
    {
        return 0;
    }

    // Layout: a grid of every glyph (16 per row, each in a tinted cell), then sample text.
    int32_t count = static_cast<int32_t>(pixel_font_glyph_count());
    constexpr int32_t COLUMNS = 16;
    int32_t cell = (PIXEL_FONT_CELL_HEIGHT + 2) * scale;   // square cells, tall enough for accents
    int32_t grid_rows = (count + COLUMNS - 1) / COLUMNS;
    int32_t margin = 4 * scale;

    static const char* const SAMPLE =
        "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG.\n"
        "0123456789  +-*/=  ()[]{}<>  !?.,:;  @#$%&_\n"
        "\n"
        "GAME  CONFIG  CHEAT  NETPLAY  MISC\n"
        "LOAD GAME...      F2\n"
        "SAVE STATE  SLOT 0 [OK]\n"
        "VIDEO MODE: 720X480 (3:2)\n"
        "SOUND: 48000 HZ STEREO\n"
        "\xE2\x96\xB2 \xE2\x96\xBC \xE2\x97\x80 \xE2\x96\xB6 \xE2\x9C\x94  ARROWS, CHECK\n"
        "lowercase draws as capitals\n"
        "\n"
        "\xC2\xBFQU\xC3\x89 TAL? \xC2\xA1SE GUARD\xC3\x93 LA PARTIDA! A\xC3\x91O, PING\xC3\x9CINO\n"
        "Fran\xC3\xA7" "ais: \xC3\xA0 \xC3\xA2 \xC3\xA8 \xC3\xAA \xC3\xAB \xC3\xAE \xC3\xB4 \xC3\xB9 \xC5\x93 \xC2\xAB OK \xC2\xBB\n"
        "Portugu\xC3\xAAs: S\xC3\x83O JO\xC3\x83O, A\xC3\x87\xC3\x83O, P\xC3\x95" "E\n"
        "Deutsch: \xC3\x84 \xC3\x96 \xC3\x9C STRA\xC3\x9F" "E  10 \xE2\x82\xAC  20\xC2\xB0";
    int32_t sample_width = 0;
    int32_t sample_height = 0;
    pixel_font_measure(SAMPLE, scale, &sample_width, &sample_height);

    int32_t width = margin * 2 + (COLUMNS * cell > sample_width ? COLUMNS * cell : sample_width);
    int32_t height = margin * 3 + grid_rows * cell + sample_height;
    uint32_t* pixels = new uint32_t[static_cast<size_t>(width) * static_cast<size_t>(height)];
    for (size_t i = 0; i < static_cast<size_t>(width) * static_cast<size_t>(height); i++)
    {
        pixels[i] = BACKGROUND;
    }

    for (int32_t i = 0; i < count; i++)
    {
        int32_t cell_x = margin + (i % COLUMNS) * cell;
        int32_t cell_y = margin + (i / COLUMNS) * cell;
        for (int32_t y = cell_y; y < cell_y + cell - scale; y++)
        {
            for (int32_t x = cell_x; x < cell_x + cell - scale; x++)
            {
                pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = CELL_COLOR;
            }
        }
        // The glyph as UTF-8 (up to 3 bytes for what the font has).
        uint32_t codepoint = pixel_font_glyph_codepoint(static_cast<uint32_t>(i));
        char text[5] = {};
        if (codepoint < 0x80)
        {
            text[0] = static_cast<char>(codepoint);
        }
        else if (codepoint < 0x800)
        {
            text[0] = static_cast<char>(0xC0u | (codepoint >> 6));
            text[1] = static_cast<char>(0x80u | (codepoint & 0x3Fu));
        }
        else
        {
            text[0] = static_cast<char>(0xE0u | (codepoint >> 12));
            text[1] = static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu));
            text[2] = static_cast<char>(0x80u | (codepoint & 0x3Fu));
        }
        pixel_font_draw_text(pixels, width, height, width, cell_x + scale, cell_y + scale, text, TEXT_COLOR, scale);
    }

    pixel_font_draw_text(pixels, width, height, width, margin, margin * 2 + grid_rows * cell, SAMPLE, DIM_COLOR, scale);

    build_crc_table();
    bool ok = write_png(output, pixels, width, height);
    delete[] pixels;
    if (!ok)
    {
        std::fprintf(stderr, "could not write %s\n", output);
        return 1;
    }
    std::printf("wrote %s (%dx%d, scale %d)\n", output, width, height, scale);
    return 0;
}
