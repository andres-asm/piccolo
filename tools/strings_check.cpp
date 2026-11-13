// strings_check: piccolo's UI text against its translations and its font
// (CTest). Catches drift: a key added to strings.txt but not translated, a
// translation for a key that no longer exists, or a character the pixel font
// can't draw (it would show as a box).
//
// Usage: piccolo_strings_check <lang directory> <code>...
// Exit code 0 = all checks passed. Each failure is printed.

#include <cstdio>
#include <cstring>
#include <string>

#include "localization.h"
#include "piccolo_strings.h"
#include "pixel_font.h"

static int g_failures = 0;

// Every character of `text` has a glyph (line breaks aside).
static void check_glyphs(const char* text, const char* key, const char* language)
{
    const char* cursor = text;
    uint32_t codepoint = pixel_font_next_codepoint(&cursor);
    while (codepoint != 0)
    {
        uint8_t rows[PIXEL_FONT_CELL_HEIGHT] = {};
        if (codepoint != '\n' && !pixel_font_glyph_rows(codepoint, rows))
        {
            std::printf("FAIL %s %s: U+%04X has no glyph in the pixel font\n", language, key, codepoint);
            g_failures++;
        }
        codepoint = pixel_font_next_codepoint(&cursor);
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::printf("usage: %s <lang directory> <code>...\n", argv[0]);
        return 2;
    }
    if (!pixel_font_init())
    {
        return 1;
    }

    const LocalizedString* table = piccolo_strings_table();
    uint32_t count = static_cast<uint32_t>(Str::count);
    Localization strings = {};
    if (!localization_init(&strings, table, count))
    {
        return 1;
    }
    for (uint32_t i = 0; i < count; i++)
    {
        check_glyphs(table[i].english, table[i].key, "en");
    }

    for (int i = 2; i < argc; i++)
    {
        const char* code = argv[i];
        std::string path = std::string(argv[1]) + "/" + code + ".po";
        if (!localization_load_po(&strings, path.c_str(), code))
        {
            std::printf("FAIL could not read %s\n", path.c_str());
            g_failures++;
            continue;
        }
        // Language names are shown in their own language: not translated.
        for (uint32_t id = 0; id < count; id++)
        {
            bool language_name = std::strncmp(table[id].key, "value_language_", 15) == 0 &&
                                 std::strcmp(table[id].key, "value_language_auto") != 0;
            if (strings.translations[id].empty() && !language_name)
            {
                std::printf("FAIL %s: %s is not translated\n", code, table[id].key);
                g_failures++;
            }
            check_glyphs(localization_get(&strings, id), table[id].key, code);
        }
    }

    if (g_failures == 0)
    {
        std::printf("strings: all checks passed (%u strings)\n", count);
        return 0;
    }
    std::printf("strings: %d check(s) failed\n", g_failures);
    return 1;
}
