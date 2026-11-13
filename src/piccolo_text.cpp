#include "piccolo_text.h"

#include "libretro.h"
#include "settings.h"

const char* piccolo_text(const Localization* strings, Str id)
{
    return localization_get(strings, text_id(id));
}

const char* piccolo_text_id(const Localization* strings, uint32_t id)
{
    return id == SETTING_NO_TEXT ? "" : localization_get(strings, id);
}

const char* piccolo_language_code(uint32_t language)
{
    return language == 1 ? "es" : "en";
}

uint32_t piccolo_language_retro(uint32_t language)
{
    return language == 1 ? RETRO_LANGUAGE_SPANISH : RETRO_LANGUAGE_ENGLISH;
}
