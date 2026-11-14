// piccolo for Android, step 1: open the window and report what Android gives us.
//
// SDLActivity (Java) starts a thread and calls SDL_main() here; SDL_main.h
// renames main() to SDL_main on Android. Nothing is drawn yet: the point is
// that the activity opens, the window exists, and the log (adb logcat -s piccolo)
// shows the window's pixel size next to the display modes. On Android TV that
// tells us whether we get a 1080p window on a 4K display.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <android/log.h>

static const char* LOG_TAG = "piccolo";

static void log_display_modes(SDL_DisplayID display)
{
    const SDL_DisplayMode* current = SDL_GetCurrentDisplayMode(display);
    if (current != nullptr) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "current display mode: %dx%d @ %.3f Hz, density %.2f",
                            current->w, current->h, static_cast<double>(current->refresh_rate),
                            static_cast<double>(current->pixel_density));
    }

    int32_t mode_count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display, &mode_count);
    if (modes == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "SDL_GetFullscreenDisplayModes failed: %s", SDL_GetError());
        return;
    }
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%d fullscreen display modes:", mode_count);
    for (int32_t i = 0; i < mode_count; i++) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "  %dx%d @ %.3f Hz", modes[i]->w, modes[i]->h,
                            static_cast<double>(modes[i]->refresh_rate));
    }
    SDL_free(modes);
}

static void log_window_size(SDL_Window* window, const char* reason)
{
    int32_t width = 0;
    int32_t height = 0;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "window size in pixels (%s): %dx%d", reason, width, height);
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "piccolo starting");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // Vulkan window from the start: that's what piccolo renders with.
    SDL_Window* window = SDL_CreateWindow("piccolo", 0, 0, SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN);
    if (window == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    log_display_modes(SDL_GetDisplayForWindow(window));
    log_window_size(window, "created");

    bool running = true;
    while (running) {
        SDL_Event event;
        // Block: nothing to draw yet, so don't spin while waiting for events.
        if (!SDL_WaitEvent(&event)) {
            continue;
        }
        switch (event.type) {
        case SDL_EVENT_QUIT:
            running = false;
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            log_window_size(window, "changed");
            break;
        case SDL_EVENT_DID_ENTER_BACKGROUND:
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "entered background");
            break;
        case SDL_EVENT_DID_ENTER_FOREGROUND:
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "entered foreground");
            break;
        default:
            break;
        }
    }

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "piccolo quitting");
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
