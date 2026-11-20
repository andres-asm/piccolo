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
typedef void (*MainLoopSave)(void* data, bool unloading);
typedef void (*MainLoopOpen)(void* data, const char* path);

// Right after the window exists. Web: the canvas fills the page and follows
// its size (piccolo's window size settings don't apply there). Desktop: nothing.
void main_loop_prepare_window(SDL_Window* window);

// Calls frame(data) until it returns false (desktop), or hands it to the
// browser for good (web: the code after this call never runs).
void main_loop_run(MainLoopFrame frame, void* data);

// The browser never runs piccolo's shutdown (closing the tab just stops it):
// save(data) is called, right then, whenever the page is hidden (tab
// switched, minimized, closed on a phone) or unloaded (reload, close), to
// write what would otherwise be written at exit (SRAM, settings);
// `unloading` is true only for the unload, the one sure end. Desktop:
// nothing (piccolo saves at exit).
void main_loop_on_leave(MainLoopSave save, void* data);

// The browser: the page opens files (its LOAD button, a file dropped on it)
// by writing them into the virtual filesystem and calling the exported
// piccolo_web_open(path), which calls open(data, path). Desktop: nothing
// (LOAD GAME browses the real filesystem).
void main_loop_on_open(MainLoopOpen open, void* data);

// True in the browser: the window is the page's size, piccolo's window
// size settings don't apply.
bool main_loop_window_follows_page();

// False in the browser: piccolo's own waits (frame timing, frame delay) must
// be skipped there.
bool main_loop_can_wait();
