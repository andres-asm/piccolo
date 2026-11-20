#pragma once

// Who drives piccolo's loop.
//
// Desktop and Android (main_loop.cpp): a plain loop, main_loop_run returns
// when the frame function says to stop, and piccolo paces itself (vsync
// presents, frame timing waits, frame delay).
//
// The browser (main_loop_web.cpp): the page must get control back after
// every frame, so the browser calls the frame function from its animation
// frames (requestAnimationFrame, at the display's refresh) and
// main_loop_run never returns. Nothing may block there: the browser paces.

struct SDL_Window;

typedef bool (*MainLoopFrame)(void* data);

// Right after the window exists. Web: the canvas fills the page and follows
// its size (piccolo's window size settings don't apply there). Desktop: nothing.
void main_loop_prepare_window(SDL_Window* window);

// Calls frame(data) until it returns false (desktop), or hands it to the
// browser for good (web: the code after this call never runs).
void main_loop_run(MainLoopFrame frame, void* data);

// True in the browser: the window is the page's size, piccolo's window
// size settings don't apply.
bool main_loop_window_follows_page();

// False in the browser: piccolo's own waits (frame timing, frame delay) must
// be skipped there.
bool main_loop_can_wait();
