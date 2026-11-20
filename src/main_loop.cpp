#include "main_loop.h"

// Desktop and Android: piccolo's own loop (see main_loop.h).

void main_loop_prepare_window(SDL_Window* window)
{
    (void)window;
}

void main_loop_run(MainLoopFrame frame, void* data)
{
    while (frame(data))
    {
    }
}

bool main_loop_window_follows_page()
{
    return false;
}

bool main_loop_can_wait()
{
    return true;
}
