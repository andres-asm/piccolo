#include "clipboard.h"

#include <emscripten/emscripten.h>

// On the page's main thread (piccolo's loop runs there). Only on https pages
// (or localhost): elsewhere navigator.clipboard doesn't exist and nothing happens.
EM_JS(void, clipboard_web_write, (const char* text), {
    if (navigator.clipboard && navigator.clipboard.writeText)
    {
        navigator.clipboard.writeText(UTF8ToString(text)).catch(function(error) {
            console.warn('piccolo: clipboard: ' + error);
        });
    }
});

void clipboard_set_text(const char* text)
{
    clipboard_web_write(text);
}
