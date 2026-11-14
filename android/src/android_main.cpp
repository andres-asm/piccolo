// piccolo for Android, step 2: draw with the shared frontend code.
//
// SDLActivity (Java) starts a thread and calls SDL_main() here; SDL_main.h
// renames main() to SDL_main on Android. app_shell (frontends/common) opens
// the window and sets up Vulkan, the swapchain, the renderer and Dear ImGui,
// the same as on desktop. On the Tegra X1 (Shield) that runs the Vulkan 1.1
// path. The screen shows what we got (GPU, Vulkan path, swapchain size, frame
// rate) next to ImGui's demo window; logcat (adb logcat -s piccolo SDL/GPU)
// has the same plus the display modes.
//
// The app going to the background (Android destroys the window's surface)
// is handled inside app_shell: it releases the swapchain and surface and
// creates them again when the app is back, while SDL blocks this loop.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <android/log.h>

#include "imgui.h"

#include "app_shell.h"

static const char* LOG_TAG = "piccolo";

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

static void draw_status_window(const AppShell* shell)
{
    const VulkanContext* vulkan = &shell->vulkan;
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("piccolo (Android)");
    ImGui::Text("GPU: %s", vulkan->physical_device_properties.deviceName);
    ImGui::Text("Device: Vulkan %u.%u.%u", VK_API_VERSION_MAJOR(vulkan->physical_device_properties.apiVersion),
                VK_API_VERSION_MINOR(vulkan->physical_device_properties.apiVersion),
                VK_API_VERSION_PATCH(vulkan->physical_device_properties.apiVersion));
    ImGui::Text("Path: %s", vulkan_path_to_string(vulkan->path));
    ImGui::Text("Swapchain: %ux%u, %u images, %s", shell->swapchain.extent.width, shell->swapchain.extent.height,
                shell->swapchain.image_count, vk_present_mode_to_string(shell->swapchain.present_mode));
    ImGui::Text("Present wait: %u", vulkan->present_wait_version);
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("%.1f fps (%.2f ms)", static_cast<double>(io.Framerate),
                static_cast<double>(1000.0f / (io.Framerate > 0.0f ? io.Framerate : 1.0f)));
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

    AppShell shell = {};
    if (!app_shell_create(&shell, &config))
    {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "app_shell_create failed (see the SDL log lines)");
        app_shell_destroy(&shell);
        return 1;
    }
    log_display_modes(SDL_GetDisplayForWindow(shell.window));

    // ImGui would try to write imgui.ini in the current folder ("/" on Android).
    ImGui::GetIO().IniFilename = nullptr;

    ClearColor clear = { 0.10f, 0.12f, 0.16f, 1.0f };
    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            app_shell_process_event(&shell, &event);
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
        }
        if (!running)
        {
            break;
        }

        if (!app_shell_begin_frame(&shell))
        {
            continue;
        }
        draw_status_window(&shell);
        ImGui::ShowDemoWindow();
        if (!app_shell_present(&shell, clear))
        {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "app_shell_present failed");
            break;
        }
    }

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "piccolo quitting");
    app_shell_destroy(&shell);
    return 0;
}
