#include "piccolo_window.h"

#include <cmath>

#include <SDL3/SDL.h>

#include "piccolo_settings.h"
#include "window_size.h"

// The room a window can have on its display, in pixels: the usable area
// (without task bars / panels) minus the window's own frame. `density` =
// pixels per window coordinate (HiDPI).
static bool usable_area(SDL_Window* window, uint32_t* width, uint32_t* height, float* density)
{
    SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (display == 0)
    {
        display = SDL_GetPrimaryDisplay();
    }
    SDL_Rect bounds = {};
    if (!SDL_GetDisplayUsableBounds(display, &bounds))
    {
        return false;
    }
    int top = 0;
    int left = 0;
    int bottom = 0;
    int right = 0;
    SDL_GetWindowBordersSize(window, &top, &left, &bottom, &right);   // stays 0 where unknown
    *density = SDL_GetWindowPixelDensity(window);
    if (*density <= 0.0f)
    {
        *density = 1.0f;
    }
    int room_width = bounds.w - left - right;
    int room_height = bounds.h - top - bottom;
    if (room_width <= 0 || room_height <= 0)
    {
        return false;
    }
    *width = static_cast<uint32_t>(static_cast<float>(room_width) * *density);
    *height = static_cast<uint32_t>(static_cast<float>(room_height) * *density);
    return true;
}

// Sets the window's size from pixels.
static void set_window_pixels(SDL_Window* window, uint32_t width, uint32_t height, float density)
{
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED)
    {
        SDL_RestoreWindow(window);   // a maximized window ignores size requests
    }
    int window_width = static_cast<int>(std::lround(static_cast<float>(width) / density));
    int window_height = static_cast<int>(std::lround(static_cast<float>(height) / density));
    SDL_SetWindowSize(window, window_width, window_height);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Window: %ux%u pixels", width, height);
}

void piccolo_window_restore(PiccoloWindow* state, SDL_Window* window, const Settings* settings)
{
    if (!state->has_saved || !settings_get_bool(settings, PICCOLO_WINDOW_REMEMBER))
    {
        return;   // the first piccolo_window_apply() sets the mode's size
    }
    uint32_t mode = settings_get_enum(settings, PICCOLO_WINDOW_MODE);
    if (mode == PICCOLO_WINDOW_FIXED && state->saved_width > 0 && state->saved_height > 0)
    {
        SDL_SetWindowSize(window, state->saved_width, state->saved_height);
        // Counts as applied: the remembered size wins over the preset.
        state->fixed_aspect = piccolo_settings_window_aspect(settings);
        state->fixed_height = piccolo_settings_window_height(settings);
    }
    // Ignored where the desktop places windows itself (Wayland).
    SDL_SetWindowPosition(window, state->saved_x, state->saved_y);
    state->mode = mode;
    state->applied = true;
}

void piccolo_window_apply(PiccoloWindow* state, SDL_Window* window, const Settings* settings,
                          const ViewportInput* picture)
{
    bool fullscreen = settings_get_bool(settings, PICCOLO_DISPLAY_FULLSCREEN);
    uint32_t fullscreen_type = settings_get_enum(settings, PICCOLO_DISPLAY_FULLSCREEN_TYPE);
    bool was_fullscreen = state->fullscreen;
    if (fullscreen != state->fullscreen || (fullscreen && fullscreen_type != state->fullscreen_type))
    {
        if (fullscreen)
        {
            // Exclusive: the display's own (desktop) mode; borderless: none.
            const SDL_DisplayMode* display_mode = nullptr;
            if (fullscreen_type == PICCOLO_FULLSCREEN_EXCLUSIVE)
            {
                display_mode = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(window));
            }
            SDL_SetWindowFullscreenMode(window, display_mode);
            SDL_SetWindowFullscreen(window, true);
        }
        else
        {
            SDL_SetWindowFullscreen(window, false);   // SDL brings back the windowed size
        }
        state->fullscreen = fullscreen;
        state->fullscreen_type = fullscreen_type;
        const char* kind = fullscreen_type == PICCOLO_FULLSCREEN_EXCLUSIVE ? "exclusive" : "borderless";
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Fullscreen: %s", fullscreen ? kind : "off");
    }

    uint32_t room_width = 0;
    uint32_t room_height = 0;
    float density = 1.0f;
    if (!usable_area(window, &room_width, &room_height, &density))
    {
        return;
    }

    // What the mode wants (also shown in the settings window).
    uint32_t mode = settings_get_enum(settings, PICCOLO_WINDOW_MODE);
    double aspect = piccolo_settings_window_aspect(settings);
    uint32_t height = piccolo_settings_window_height(settings);
    state->target_width = 0;
    state->target_height = 0;
    state->target_scale = 0;
    if (mode == PICCOLO_WINDOW_FIXED)
    {
        window_size_fixed(aspect, height, room_width, room_height, &state->target_width, &state->target_height);
    }
    else if (picture != nullptr)
    {
        uint32_t scale = static_cast<uint32_t>(settings_get_int(settings, PICCOLO_WINDOW_SCALE));
        window_size_dynamic(picture, scale, room_width, room_height, &state->target_width, &state->target_height,
                            &state->target_scale);
    }

    // Fullscreen covers the screen; the windowed size waits until it's off
    // (or right after it was switched off: SDL restores the old size first).
    if (fullscreen || was_fullscreen)
    {
        return;
    }

    bool mode_changed = !state->applied || mode != state->mode;
    if (mode == PICCOLO_WINDOW_FIXED)
    {
        if (mode_changed || aspect != state->fixed_aspect || height != state->fixed_height)
        {
            set_window_pixels(window, state->target_width, state->target_height, density);
            state->fixed_aspect = aspect;
            state->fixed_height = height;
        }
    }
    else if (state->target_width > 0)
    {
        if (mode_changed || state->target_width != state->dynamic_width || state->target_height != state->dynamic_height)
        {
            set_window_pixels(window, state->target_width, state->target_height, density);
            state->dynamic_width = state->target_width;
            state->dynamic_height = state->target_height;
        }
    }
    state->mode = mode;
    state->applied = true;
}

void piccolo_window_capture(PiccoloWindow* state, SDL_Window* window)
{
    SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    if (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MINIMIZED | SDL_WINDOW_MAXIMIZED))
    {
        return;   // keep the windowed size we had
    }
    int width = 0;
    int height = 0;
    int x = 0;
    int y = 0;
    if (!SDL_GetWindowSize(window, &width, &height) || width <= 0 || height <= 0)
    {
        return;
    }
    SDL_GetWindowPosition(window, &x, &y);
    state->saved_width = width;
    state->saved_height = height;
    state->saved_x = x;
    state->saved_y = y;
    state->has_saved = true;
}
