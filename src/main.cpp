// piccolo: a libretro frontend for playing games, with a retro-style UI
// (retro_ui.h: themed Dear ImGui widgets with piccolo's own pixel font).
//
// The game fills the window. The menu bar (GAME / CONFIG / MISC) is shown
// when nothing runs; while a game runs, Esc shows and hides it, and the game
// pauses while it's shown (or while the LOAD GAME window is open).
//
// Running the core is frontend_libretro's core session, shared with the
// host-libretro test frontend. Cores, saves, states and settings live in
// piccolo's own folder (app_paths.h: ~/.local/share/piccolo on Linux).

#include <cstdio>
#include <cstring>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "imgui.h"

#include "app_paths.h"
#include "app_shell.h"
#include "picture.h"
#include "settings_file.h"
#include "core_catalog.h"
#include "core_session.h"
#include "file_picker.h"
#include "pixel_font.h"
#include "pixel_font_imgui.h"
#include "retro_ui.h"
#include "vk_swapchain.h"

static const ClearColor CLEAR_COLOR = { 0.0f, 0.0f, 0.0f, 1.0f };   // around the game picture

struct App
{
    AppShell shell = {};
    bool     running = false;
    bool     show_debug_window = false;
    bool     show_imgui_demo = false;

    ImFont* pixel_font = nullptr;   // piccolo's pixel font as an ImGui font
    int32_t ui_scale   = 1;         // window pixels per retro pixel
    char    status[256] = "GAME > LOAD GAME TO START";

    CoreSession session = {};
    CoreCatalog catalog = {};
    FilePicker  picker  = {};
    bool        menu_visible = true;   // only hideable while a game runs

    // What the LOAD GAME window picked, started at the top of the NEXT loop:
    // starting stops the old core, which destroys the texture this frame's
    // picture may already have been queued with (a crash at render time).
    FilePickerChoice pending_start = {};
    bool             has_pending_start = false;

    // piccolo's folder, and what was used last (<settings>/last_used.cfg).
    AppPaths    paths = {};
    Picture     picture = {};   // the game on screen (viewport + filter)
    std::string imgui_ini;   // <settings>/imgui.ini; ImGui keeps a pointer to it
    char last_core[LIBRETRO_PATH_MAX]      = {};
    char last_folder[LIBRETRO_PATH_MAX]    = {};
};

// ---------------------------------------------------------------------------
// Last used core and folder (<settings>/last_used.cfg)
// ---------------------------------------------------------------------------

static std::string last_used_file(const App* app)
{
    return app->paths.settings + "/last_used.cfg";
}

static void save_last_used(const App* app)
{
    if (app->paths.settings.empty())
    {
        return;
    }
    SettingsFile file = {};
    if (settings_file_set(&file, "", "core", app->last_core) && settings_file_set(&file, "", "folder", app->last_folder))
    {
        settings_file_save(&file, last_used_file(app).c_str(), "piccolo: last used core and folder");
    }
}

static void load_last_used(App* app)
{
    SettingsFile file = {};
    if (app->paths.settings.empty() || !settings_file_load(&file, last_used_file(app).c_str()))
    {
        return;
    }
    const char* core = settings_file_get(&file, "", "core");
    const char* folder = settings_file_get(&file, "", "folder");
    if (core != nullptr)
    {
        std::snprintf(app->last_core, sizeof(app->last_core), "%s", core);
    }
    if (folder != nullptr)
    {
        std::snprintf(app->last_folder, sizeof(app->last_folder), "%s", folder);
    }
}

// ---------------------------------------------------------------------------
// Game control
// ---------------------------------------------------------------------------

static bool game_running(const App* app)
{
    return core_session_running(&app->session);
}

static void set_status(App* app, const char* text)
{
    std::snprintf(app->status, sizeof(app->status), "%s", text);
}

// The game pauses while the menu or the LOAD GAME window is up, and its
// queued audio is dropped so resuming doesn't play stale sound.
static void update_pause(App* app)
{
    bool pause = game_running(app) && (app->menu_visible || app->picker.open);
    if (pause && !app->session.paused)
    {
        audio_output_clear(&app->session.audio);
    }
    app->session.paused = pause;
}

static void start_game(App* app, const FilePickerChoice* choice)
{
    if (core_session_start(&app->session, &app->shell, choice->core_path, choice->content_path))
    {
        app->menu_visible = false;   // straight into the game
        std::snprintf(app->last_core, sizeof(app->last_core), "%s", choice->core_path);
        if (choice->content_path[0] != '\0')
        {
            // The folder of the file, for the next LOAD GAME.
            std::snprintf(app->last_folder, sizeof(app->last_folder), "%s", choice->content_path);
            char* slash = std::strrchr(app->last_folder, '/');
            if (slash != nullptr && slash != app->last_folder)
            {
                *slash = '\0';
            }
        }
        save_last_used(app);
    }
    set_status(app, app->session.message);
}

static void handle_menu_action(App* app, MenuAction action)
{
    switch (action)
    {
        case MenuAction::none:
            return;
        case MenuAction::load_game:
            core_catalog_scan(&app->catalog, game_running(app) ? app->session.host : nullptr);
            file_picker_open(&app->picker, &app->catalog, app->last_folder, app->last_core);
            break;
        case MenuAction::reset:
            if (game_running(app))
            {
                libretro_host_reset(app->session.host);
                set_status(app, "RESET");
                app->menu_visible = false;
            }
            break;
        case MenuAction::save_state:
            if (core_session_save_state(&app->session))
            {
                app->menu_visible = false;
            }
            set_status(app, app->session.message);
            break;
        case MenuAction::load_state:
            if (core_session_load_state(&app->session))
            {
                app->menu_visible = false;
            }
            set_status(app, app->session.message);
            break;
        case MenuAction::quit:
            app->running = false;
            break;
        case MenuAction::debug_window:
            app->show_debug_window = !app->show_debug_window;
            break;
        default:
            set_status(app, "NOT THERE YET");
            break;
    }
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

// Keyboard as the game's joypad only while the game has the screen.
static bool game_has_keyboard(const App* app)
{
    return game_running(app) && !app->menu_visible && !app->picker.open;
}

static void app_handle_events(App* app)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (core_session_handle_event(&app->session, &app->shell, &event, game_has_keyboard(app)))
        {
            continue;   // the core's keyboard (game focus)
        }
        app_shell_process_event(&app->shell, &event);

        if (event.type == SDL_EVENT_QUIT)
        {
            app->running = false;
        }
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(app->shell.window))
        {
            app->running = false;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
        {
            switch (event.key.key)
            {
                case SDLK_ESCAPE:
                    // Close the LOAD GAME window first; otherwise show/hide the
                    // menu (it stays up when nothing runs).
                    if (app->picker.open)
                    {
                        app->picker.open = false;
                    }
                    else if (game_running(app))
                    {
                        app->menu_visible = !app->menu_visible;
                    }
                    break;
                case SDLK_F2:
                    handle_menu_action(app, MenuAction::save_state);
                    break;
                case SDLK_F4:
                    handle_menu_action(app, MenuAction::load_state);
                    break;
                default:
                    break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

static void draw_game(App* app)
{
    CoreSession* session = &app->session;
    app->picture.ready = false;
    if (!game_running(app) || !session->texture.has_content || session->texture_id == 0)
    {
        return;
    }
    // Picture settings come in plan-settings step 6; until then: the core's
    // aspect, as big as fits, sharp pixels, centered (what piccolo did before).
    const ImGuiIO& io = ImGui::GetIO();
    ViewportInput input = {};
    input.frame_width = session->texture.content_width;
    input.frame_height = session->texture.content_height;
    input.core_aspect = static_cast<double>(session->host->av_info.geometry.aspect_ratio);
    input.window_width = static_cast<uint32_t>(io.DisplaySize.x);
    input.window_height = static_cast<uint32_t>(io.DisplaySize.y);
    input.aspect = ViewportAspect::core;
    input.scaling = ViewportScaling::full;
    input.filter = ViewportFilter::nearest;

    ViewportResult viewport = {};
    if (viewport_compute(&input, &viewport) &&
        picture_prepare(&app->picture, &app->shell.vulkan, &session->texture, &viewport))
    {
        // The background draw list is behind every window, menu bar included.
        picture_draw(&app->picture, ImGui::GetBackgroundDrawList(), &session->texture, session->texture_id, 0.0f, 0.0f);
    }
}

static void app_draw_debug_ui(App* app)
{
    const ImGuiIO& io = ImGui::GetIO();
    const VkPhysicalDeviceProperties* gpu = &app->shell.vulkan.physical_device_properties;
    const VulkanSwapchain* swapchain = &app->shell.swapchain;

    ImGui::SetNextWindowPos(ImVec2(40.0f, 80.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("piccolo", &app->show_debug_window);

    ImGui::Text("GPU: %s", gpu->deviceName);
    ImGui::Text("Vulkan %u.%u.%u", VK_API_VERSION_MAJOR(gpu->apiVersion), VK_API_VERSION_MINOR(gpu->apiVersion),
                VK_API_VERSION_PATCH(gpu->apiVersion));
    ImGui::Text("Swapchain: %ux%u, %u images, %s", swapchain->extent.width, swapchain->extent.height,
                swapchain->image_count, vk_present_mode_to_string(swapchain->present_mode));
    ImGui::Text("Validation: %s", app->shell.vulkan.validation_enabled ? "on" : "off");
    ImGui::Text("Retro UI scale: %dx", app->ui_scale);
    ImGui::Separator();
    ImGui::Text("%.3f ms/frame (%.1f FPS)", static_cast<double>(1000.0f / io.Framerate), static_cast<double>(io.Framerate));
    if (game_running(app))
    {
        const CoreSession* session = &app->session;
        ImGui::Text("Core: %s, %.3f fps, timing %s", session->host->system_info.library_name,
                    session->host->av_info.timing.fps, frame_timing_mode_name(session->timing.effective));
        ImGui::Text("Audio: x%.5f, %u ms queued, %llu underruns", session->audio.current_ratio,
                    audio_output_queued_ms(&session->audio), static_cast<unsigned long long>(session->audio.underruns));
    }
    ImGui::Separator();
    ImGui::Checkbox("Dear ImGui demo window", &app->show_imgui_demo);

    ImGui::End();

    if (app->show_imgui_demo)
    {
        ImGui::ShowDemoWindow(&app->show_imgui_demo);
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // The session, catalog and file picker are large (paths, file lists): off the stack.
    App* app = new App{};

    AppShellConfig config = {};
    config.name       = "piccolo";
    config.identifier = "org.multitap.piccolo";

    if (!pixel_font_init() || !core_session_create(&app->session))
    {
        delete app;
        return 1;
    }
    if (!app_shell_create(&app->shell, &config))
    {
        app_shell_destroy(&app->shell);
        core_session_destroy(&app->session, &app->shell);
        delete app;
        return 1;
    }
    app->pixel_font = pixel_font_imgui_add();
    if (app->pixel_font == nullptr)
    {
        core_session_destroy(&app->session, &app->shell);
        app_shell_destroy(&app->shell);
        delete app;
        return 1;
    }

    // Everything in piccolo's folder, migrated once from ~/.host-libretro.
    // Without it nothing is saved, but games still run.
    ImGuiIO& io = ImGui::GetIO();
    if (app_paths_init(&app->paths, "piccolo", "piccolo.cfg"))
    {
        app->imgui_ini = app->paths.settings + "/imgui.ini";
        io.IniFilename = app->imgui_ini.c_str();
        std::snprintf(app->session.data_directory, sizeof(app->session.data_directory), "%s", app->paths.root.c_str());
        std::snprintf(app->session.system_directory, sizeof(app->session.system_directory), "%s",
                      app->paths.system.c_str());
        std::string cache_path = app->paths.root + "/core_info_cache.cfg";
        core_catalog_init(&app->catalog, app->paths.cores.c_str(), cache_path.c_str());
        core_catalog_scan(&app->catalog, nullptr);
        load_last_used(app);
    }
    else
    {
        io.IniFilename = nullptr;
    }

    app->running = true;
    while (app->running)
    {
        app_handle_events(app);
        if (app->has_pending_start)
        {
            app->has_pending_start = false;
            start_game(app, &app->pending_start);   // outside any frame: nothing drawn with the old texture yet
        }
        update_pause(app);
        core_session_update(&app->session, &app->shell);
        if (core_session_run_frame(&app->session, &app->shell, nullptr, game_has_keyboard(app)) == CoreSessionFrame::shut_down)
        {
            set_status(app, app->session.message);
            app->menu_visible = true;
        }

        if (app_shell_begin_frame(&app->shell))
        {
            app->ui_scale = retro_ui_scale_for_width(ImGui::GetIO().DisplaySize.x);

            draw_game(app);   // background, behind everything
            bool menu_shown = app->menu_visible || !game_running(app);
            if (menu_shown)
            {
                // The status line's window goes first, so it stays behind the others.
                retro_ui_status_line(app->pixel_font, app->ui_scale, app->status);
                handle_menu_action(app, retro_ui_menu_bar(app->pixel_font, app->ui_scale));
            }

            if (file_picker_draw(&app->picker, &app->catalog, app->pixel_font, app->ui_scale, &app->pending_start))
            {
                app->has_pending_start = true;   // see pending_start
            }
            if (app->show_debug_window)
            {
                app_draw_debug_ui(app);
            }

            core_session_upload_video(&app->session, &app->shell);
            if (app_shell_acquire(&app->shell))
            {
                picture_record(&app->picture, renderer_command_buffer(&app->shell.renderer), &app->session.texture);
            }
            if (!app_shell_present(&app->shell, CLEAR_COLOR))
            {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Fatal rendering error, exiting");
                break;
            }
        }
        core_session_wait(&app->session);
    }

    core_session_destroy(&app->session, &app->shell);   // writes save RAM
    picture_destroy(&app->picture, &app->shell.vulkan);
    app_shell_destroy(&app->shell);
    delete app;
    return 0;
}
