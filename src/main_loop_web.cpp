#include "main_loop.h"

#include <SDL3/SDL.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

// The browser: its animation frames drive piccolo's loop (see main_loop.h).

struct WebLoop
{
    MainLoopFrame frame     = nullptr;
    void*         data      = nullptr;
    MainLoopSave  save      = nullptr;
    void*         save_data = nullptr;
    MainLoopOpen  open      = nullptr;
    void*         open_data = nullptr;
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

// Both run on the main thread during the browser's event, so what save()
// writes is on OPFS before the page goes (storage.h).
static bool on_visibility_change(int event_type, const EmscriptenVisibilityChangeEvent* event, void* data)
{
    (void)event_type;
    (void)data;
    if (event->hidden && g_loop.save != nullptr)
    {
        g_loop.save(g_loop.save_data);
    }
    return false;
}

static const char* on_before_unload(int event_type, const void* reserved, void* data)
{
    (void)event_type;
    (void)reserved;
    (void)data;
    if (g_loop.save != nullptr)
    {
        g_loop.save(g_loop.save_data);
    }
    return nullptr;   // no "leave this page?" question
}

void main_loop_on_leave(MainLoopSave save, void* data)
{
    g_loop.save = save;
    g_loop.save_data = data;
    emscripten_set_visibilitychange_callback(nullptr, false, on_visibility_change);
    emscripten_set_beforeunload_callback(nullptr, on_before_unload);
}

void main_loop_on_open(MainLoopOpen open, void* data)
{
    g_loop.open = open;
    g_loop.open_data = data;
}

// Called by the page (web/shell.html) with a file it wrote into the virtual
// filesystem. On the main thread, between frames.
extern "C" EMSCRIPTEN_KEEPALIVE void piccolo_web_open(const char* path)
{
    if (g_loop.open != nullptr)
    {
        g_loop.open(g_loop.open_data, path);
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
