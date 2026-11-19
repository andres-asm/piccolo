#pragma once

// piccolo's UI text: the string IDs (Str, generated from strings.txt) and
// the lookup in the current language (localization.h). Every text piccolo
// shows goes through here.

#include <cstdint>

#include "localization.h"
#include "piccolo_strings.h"   // generated: enum class Str, piccolo_strings_table()

// A Str as the plain number the settings tables store.
constexpr uint32_t text_id(Str id)
{
    return static_cast<uint32_t>(id);
}

// The text for `id` in the current language (English when untranslated).
const char* piccolo_text(const Localization* strings, Str id);

// Same for a stored ID; "" for SETTING_NO_TEXT.
const char* piccolo_text_id(const Localization* strings, uint32_t id);

// Languages piccolo has (English is built in; the others are lang/<code>.po).
constexpr uint32_t PICCOLO_LANGUAGE_COUNT = 2;
const char* piccolo_language_code(uint32_t language);   // "en", "es"
uint32_t piccolo_language_retro(uint32_t language);     // RETRO_LANGUAGE_* for cores

// The language with `code` ("es"), or PICCOLO_LANGUAGE_COUNT if piccolo
// doesn't have it.
uint32_t piccolo_language_from_code(const char* code);

// Loads `language` into `strings` (0 = English only): lang/<code>.po, the
// user's copy in piccolo's folder (`root`, may be "") winning over the one
// that comes with piccolo, next to the program.
void piccolo_text_load_language(Localization* strings, uint32_t language, const char* root);
