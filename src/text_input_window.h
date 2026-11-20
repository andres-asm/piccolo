#pragma once

// The text a core asks for (Multitap's text input extension, TEXT mode;
// libretro_extensions.h in host-libretro). Open while the request is
// pending; the game pauses meanwhile.
//
//   +- ENTER TEXT ----------------------+
//   | NAME                              |   <- the core's prompt
//   | [LINK____________]                |
//   | [OK] [CANCEL]                     |
//   +-----------------------------------+
//
// On a phone, Dear ImGui's field opens the on-screen keyboard by itself.

#include <cstdint>

#include "libretro_host.h"
#include "localization.h"

struct ImFont;

struct TextInputWindow
{
    uint32_t serial = 0;                         // the request `buffer` belongs to
    char     buffer[LIBRETRO_TEXT_INPUT_MAX] = {};
    bool     focus_field = false;                // put the cursor in the field on the next draw
};

enum class TextInputChoice
{
    none,     // still open
    ok,       // answer with window->buffer
    cancel,   // answer with nothing
};

// Draws the box for `request` (mode TEXT), modal and centered. A new request
// (another serial) starts again from its initial text.
TextInputChoice text_input_window_draw(TextInputWindow* window, const LibretroTextInput* request, ImFont* pixel_font,
                                       int32_t scale, const Localization* strings);
