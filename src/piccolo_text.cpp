#include "piccolo_text.h"

#include <cstring>
#include <string>

#include <SDL3/SDL.h>

#include "host_fs.h"
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

uint32_t piccolo_language_from_code(const char* code)
{
    for (uint32_t i = 0; i < PICCOLO_LANGUAGE_COUNT; i++)
    {
        if (std::strcmp(code, piccolo_language_code(i)) == 0)
        {
            return i;
        }
    }
    return PICCOLO_LANGUAGE_COUNT;
}

void piccolo_text_load_language(Localization* strings, uint32_t language, const char* root)
{
    if (language == 0 || language >= PICCOLO_LANGUAGE_COUNT)
    {
        localization_use_english(strings);
        return;
    }
    std::string file = std::string("lang/") + piccolo_language_code(language) + ".po";
    std::string user_copy = root[0] == '\0' ? std::string() : std::string(root) + "/" + file;
    const char* base = SDL_GetBasePath();   // ends with a separator; SDL owns it
    std::string bundled = base != nullptr ? std::string(base) + file : file;
    host_fs_forward_slashes(bundled.data());
    const std::string* path = !user_copy.empty() && host_fs_exists(user_copy.c_str()) ? &user_copy : &bundled;
    localization_load_po(strings, path->c_str(), piccolo_language_code(language));
}
