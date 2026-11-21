#include "clipboard.h"

#include <SDL3/SDL.h>

void clipboard_set_text(const char* text)
{
    SDL_SetClipboardText(text);
}
