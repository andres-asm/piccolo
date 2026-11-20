#include "main_loop.h"

#include <SDL3/SDL.h>
#include <emscripten/emscripten.h>

// The browser: its animation frames drive piccolo's loop (see main_loop.h).

struct WebLoop
{
    MainLoopFrame frame = nullptr;
    void*         data  = nullptr;
};

static WebLoop g_loop = {};

void main_loop_prepare_window(SDL_Window* window)
{
    if (!SDL_SetWindowFillDocument(window, true))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "The canvas can't fill the page: %s", SDL_GetError());
    }
}

static void web_frame(void* data)
{
    (void)data;
    if (!g_loop.frame(g_loop.data))
    {
        // QUIT (or a fatal error): no more frames. The page stays as it is.
        emscripten_cancel_main_loop();
    }
}

void main_loop_run(MainLoopFrame frame, void* data)
{
    g_loop.frame = frame;
    g_loop.data = data;
    // fps 0: requestAnimationFrame. simulate_infinite_loop: main() doesn't
    // go on (its shutdown code would run with the game still going).
    emscripten_set_main_loop_arg(web_frame, nullptr, 0, true);
}

bool main_loop_window_follows_page()
{
    return true;
}

bool main_loop_can_wait()
{
    return false;
}
