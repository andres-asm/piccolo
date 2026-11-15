// piccolo for Android, step 3: run a libretro core.
//
// SDLActivity (Java) starts a thread and calls SDL_main() here; SDL_main.h
// renames main() to SDL_main on Android. app_shell (frontends/common) opens
// the window and sets up Vulkan, the swapchain, the renderer and Dear ImGui;
// frontend_libretro's core session runs the core (video, audio, timing,
// input) and picture/viewport draw its frame, all the same as piccolo on
// desktop. The core is Test libretro, shipped in the APK as
// lib/arm64-v8a/libtest_libretro.so (see build-apk.sh). A small window shows
// the GPU, the Vulkan path, the core and the frame rate; logcat has the rest
// (adb logcat -s piccolo SDL/GPU SDL/APP).
//
// The app going to the background (Android destroys the window's surface)
// is handled inside app_shell: it releases the swapchain and surface and
// creates them again when the app is back, while SDL blocks this loop.

#include <cstdio>
#include <cstring>

#include <dlfcn.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <android/log.h>

#include "imgui.h"

#include "app_shell.h"
#include "core_session.h"
#include "host_fs.h"
#include "picture.h"
#include "viewport.h"

static const char* LOG_TAG = "piccolo";

// Shipped next to libmain.so (Android extracts lib/<abi>/lib*.so there).
static const char* TEST_CORE_FILE = "libtest_libretro.so";

static void log_display_modes(SDL_DisplayID display)
{
    const SDL_DisplayMode* current = SDL_GetCurrentDisplayMode(display);
    if (current != nullptr)
    {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "current display mode: %dx%d @ %.3f Hz, density %.2f",
                            current->w, current->h, static_cast<double>(current->refresh_rate),
                            static_cast<double>(current->pixel_density));
    }
}

// The folder Android extracted our native libraries to: the one libmain.so
// was loaded from. dladdr on something inside libmain.so tells us its path.
static bool native_library_directory(char* out, size_t out_size)
{
    Dl_info info = {};
    if (dladdr(&TEST_CORE_FILE, &info) == 0 || info.dli_fname == nullptr)
    {
        return false;
    }
    const char* slash = std::strrchr(info.dli_fname, '/');
    if (slash == nullptr)
    {
        return false;
    }
    size_t length = static_cast<size_t>(slash - info.dli_fname);
    if (length + 1 > out_size)
    {
        return false;
    }
    std::memcpy(out, info.dli_fname, length);
    out[length] = '\0';
    return true;
}

// saves/, states/ and system/ in the app's internal storage
// (/data/data/<package>/files), which only the app can see.
static void set_session_directories(CoreSession* session)
{
    const char* base = SDL_GetAndroidInternalStoragePath();
    if (base == nullptr)
    {
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "No internal storage path: %s", SDL_GetError());
        return;
    }
    std::snprintf(session->saves_directory, sizeof(session->saves_directory), "%s/saves", base);
    std::snprintf(session->states_directory, sizeof(session->states_directory), "%s/states", base);
    std::snprintf(session->system_directory, sizeof(session->system_directory), "%s/system", base);
    host_fs_make_directories(session->saves_directory);
    host_fs_make_directories(session->states_directory);
    host_fs_make_directories(session->system_directory);
}

// The core's frame, behind everything, the way piccolo draws it on desktop
// (viewport defaults: the core's aspect, automatic scaling, nearest).
static void draw_game(CoreSession* session, Picture* picture, AppShell* shell)
{
    picture->ready = false;
    if (!core_session_running(session) || !session->texture.has_content || session->texture_id == 0)
    {
        return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    ViewportInput input = {};
    input.frame_width = session->texture.content_width;
    input.frame_height = session->texture.content_height;
    input.core_aspect = static_cast<double>(session->host->av_info.geometry.aspect_ratio);
    input.window_width = static_cast<uint32_t>(io.DisplaySize.x);
    input.window_height = static_cast<uint32_t>(io.DisplaySize.y);

    ViewportResult viewport = {};
    if (viewport_compute(&input, &viewport) &&
        picture_prepare(picture, &shell->vulkan, &session->texture, &viewport))
    {
        picture_draw(picture, ImGui::GetBackgroundDrawList(), &session->texture, session->texture_id, 0.0f, 0.0f);
    }
}

static void draw_status_window(const AppShell* shell, const CoreSession* session)
{
    const VulkanContext* vulkan = &shell->vulkan;
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.6f);
    ImGui::Begin("piccolo (Android)", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("GPU: %s", vulkan->physical_device_properties.deviceName);
    ImGui::Text("Path: %s", vulkan_path_to_string(vulkan->path));
    ImGui::Text("Swapchain: %ux%u, %s", shell->swapchain.extent.width, shell->swapchain.extent.height,
                vk_present_mode_to_string(shell->swapchain.present_mode));
    ImGui::Text("%s", session->message);
    if (core_session_running(session))
    {
        ImGui::Text("Core: %ux%u @ %.2f fps", session->texture.content_width, session->texture.content_height,
                    session->host->av_info.timing.fps);
    }
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Screen: %.1f fps", static_cast<double>(io.Framerate));
    ImGui::End();
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "piccolo starting");

    AppShellConfig config = {};
    config.name = "piccolo";
    config.version = "0.1.0";
    config.identifier = "io.github.andres_asm.piccolo";

    CoreSession session = {};
    AppShell shell = {};
    Picture picture = {};
    if (!core_session_create(&session) || !app_shell_create(&shell, &config))
    {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "startup failed (see the SDL log lines)");
        core_session_destroy(&session, &shell);
        app_shell_destroy(&shell);
        return 1;
    }
    log_display_modes(SDL_GetDisplayForWindow(shell.window));

    // ImGui would try to write imgui.ini in the current folder ("/" on Android).
    ImGui::GetIO().IniFilename = nullptr;

    set_session_directories(&session);
    char library_directory[LIBRETRO_PATH_MAX] = {};
    char core_path[LIBRETRO_PATH_MAX] = {};
    if (native_library_directory(library_directory, sizeof(library_directory)))
    {
        std::snprintf(core_path, sizeof(core_path), "%s/%s", library_directory, TEST_CORE_FILE);
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "core: %s", core_path);
        core_session_start(&session, &shell, core_path, nullptr);
    }
    else
    {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "could not find our native library folder");
    }
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", session.message);

    ClearColor clear = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
            if (!core_session_handle_event(&session, &shell, &event, true))
            {
                app_shell_process_event(&shell, &event);
            }
        }
        if (!running)
        {
            break;
        }

        core_session_update(&session, &shell);
        if (core_session_run_frame(&session, &shell, nullptr, true) == CoreSessionFrame::shut_down)
        {
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", session.message);
        }

        if (app_shell_begin_frame(&shell))
        {
            draw_game(&session, &picture, &shell);
            draw_status_window(&shell, &session);

            core_session_upload_video(&session, &shell);
            if (app_shell_acquire(&shell))
            {
                picture_record(&picture, renderer_command_buffer(&shell.renderer), &session.texture);
            }
            if (!app_shell_present(&shell, clear))
            {
                __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "app_shell_present failed");
                break;
            }
        }
        core_session_wait(&session);
    }

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "piccolo quitting");
    core_session_destroy(&session, &shell);   // writes save RAM
    picture_destroy(&picture, &shell.vulkan);
    app_shell_destroy(&shell);
    return 0;
}
