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
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "imgui.h"

#include "app_paths.h"
#include "app_shell.h"
#include "frame_delay.h"
#include "game_mode.h"
#include "picture.h"
#include "piccolo_settings.h"
#include "piccolo_text.h"
#include "piccolo_window.h"
#include "recent_games.h"
#include "settings_window.h"
#include "host_fs.h"
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
    char    status[256] = "";   // set at startup, in the chosen language

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

    // Settings (piccolo_settings.h) in <settings>/piccolo.cfg, and their window.
    Settings       settings = {};
    std::string    settings_path;
    SettingsWindow settings_window = {};
    bool           settings_window_was_open = false;
    std::string    cores_directory;   // the one the catalog lists now
    char           core_name[256] = {};   // for the core/game settings layers ("" = none)
    char           game_name[256] = {};

    PiccoloWindow window = {};   // Display settings applied to the window

    // The RECENT menu: the games (<settings>/recent.cfg) and their labels,
    // rebuilt when the list or the language changes (not every frame).
    RecentGames              recent = {};
    std::string              recent_path;
    std::vector<std::string> recent_labels;
    std::vector<const char*> recent_label_pointers;

    // UI text (piccolo_text.h) and the language setting it was loaded for.
    Localization strings = {};
    uint32_t     language_setting = UINT32_MAX;

    // Playback devices connected now, for the Sound tab: listed at start and
    // again when one comes or goes (not every frame).
    bool                     audio_ready = false;   // we initialized SDL's audio subsystem
    std::vector<std::string> audio_devices;
    std::vector<const char*> audio_device_pointers;

    // AUTO audio latency learned per core (<settings>/audio_latency.cfg,
    // "core = ms"): state, not a setting. A game on that core starts its
    // automatic target there instead of from the bottom again.
    SettingsFile audio_learned;
    std::string  audio_learned_path;
    bool         audio_learned_dirty = false;
    uint32_t     game_starts = 0;            // counts start_game calls
    uint32_t     learned_for_start = 0;      // the start the values below belong to (0 = none)
    uint32_t     learned_start_ms = 0;       // where this game's AUTO target starts (0 = the default)
    uint32_t     learned_recorded_ms = 0;    // the last target written to audio_learned

    // AUTO FRAME DELAY (frame_delay.h): the delay it found, for this game.
    AutoFrameDelay auto_delay = {};
    uint64_t       delay_last_ns = 0;      // last loop it was fed (0 = not applying)
    uint32_t       frame_delay_ms = 0;     // the delay used last loop
    // The locked AUTO delay per game (<settings>/frame_delay.cfg, a section
    // per "core/game", "delay_ms = N"): state, not a setting. A game found
    // before starts locked at it; SEARCH AGAIN forgets it.
    SettingsFile   frame_delay_saved;
    std::string    frame_delay_saved_path;

    // Core options per core / folder / game (settings/core_options.cfg) and
    // what each core declared (settings/core_options_cache.cfg), for the
    // session and the CORE OPTIONS tab.
    CoreOptionsStore option_store;
    CoreOptionsCache option_cache;
    std::string      option_store_path;
    std::string      option_cache_path;

    GameMode game_mode = {};   // held while a game runs (GAME MODE setting)

    bool window_focused     = true;
    bool screensaver_allowed = false;   // what we last told SDL
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
    bool ok = settings_file_set(&file, "", "core", app->last_core) && settings_file_set(&file, "", "folder", app->last_folder);
    if (ok && app->window.has_saved)
    {
        char number[16] = {};
        std::snprintf(number, sizeof(number), "%d", app->window.saved_x);
        ok = settings_file_set(&file, "", "window_x", number);
        std::snprintf(number, sizeof(number), "%d", app->window.saved_y);
        ok = ok && settings_file_set(&file, "", "window_y", number);
        std::snprintf(number, sizeof(number), "%d", app->window.saved_width);
        ok = ok && settings_file_set(&file, "", "window_width", number);
        std::snprintf(number, sizeof(number), "%d", app->window.saved_height);
        ok = ok && settings_file_set(&file, "", "window_height", number);
    }
    if (ok)
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
    const char* x = settings_file_get(&file, "", "window_x");
    const char* y = settings_file_get(&file, "", "window_y");
    const char* width = settings_file_get(&file, "", "window_width");
    const char* height = settings_file_get(&file, "", "window_height");
    if (x != nullptr && y != nullptr && width != nullptr && height != nullptr)
    {
        app->window.saved_x = static_cast<int32_t>(std::strtol(x, nullptr, 10));
        app->window.saved_y = static_cast<int32_t>(std::strtol(y, nullptr, 10));
        app->window.saved_width = static_cast<int32_t>(std::strtol(width, nullptr, 10));
        app->window.saved_height = static_cast<int32_t>(std::strtol(height, nullptr, 10));
        app->window.has_saved = app->window.saved_width > 0 && app->window.saved_height > 0;
    }
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

// A folder setting, or the program folder's own when it's empty.
static std::string folder_setting(const App* app, const char* key, const std::string& default_folder)
{
    const char* value = settings_get_text(&app->settings, key);
    return value[0] != '\0' ? std::string(value) : default_folder;
}

// Folders from the settings: the catalog's cores folder now, the others for
// the next game the session starts.
static void apply_folders(App* app)
{
    if (app->paths.root.empty())
    {
        return;
    }
    std::string saves = folder_setting(app, PICCOLO_PATH_SAVES, app->paths.saves);
    std::string states = folder_setting(app, PICCOLO_PATH_STATES, app->paths.states);
    std::string system = folder_setting(app, PICCOLO_PATH_SYSTEM, app->paths.system);
    std::snprintf(app->session.saves_directory, sizeof(app->session.saves_directory), "%s", saves.c_str());
    std::snprintf(app->session.states_directory, sizeof(app->session.states_directory), "%s", states.c_str());
    std::snprintf(app->session.system_directory, sizeof(app->session.system_directory), "%s", system.c_str());

    std::string cores = folder_setting(app, PICCOLO_PATH_CORES, app->paths.cores);
    if (cores != app->cores_directory)
    {
        app->cores_directory = cores;
        std::string cache_path = app->paths.root + "/core_info_cache.cfg";
        core_catalog_init(&app->catalog, cores.c_str(), cache_path.c_str());
        core_catalog_scan(&app->catalog, nullptr);
    }
}

static void save_settings(App* app)
{
    if (app->settings.dirty && !app->settings_path.empty())
    {
        settings_save(&app->settings, app->settings_path.c_str(), "piccolo settings");
    }
}

// The core/game layers follow what runs: core = the core's short name,
// game = the content file's name (the same names the save files use).
static void update_settings_context(App* app)
{
    app->core_name[0] = '\0';
    app->game_name[0] = '\0';
    const LibretroHost* host = app->session.host;
    if (core_session_running(&app->session))
    {
        libretro_core_short_name(host->core_path, app->core_name, sizeof(app->core_name));
        if (host->content_path[0] != '\0')
        {
            host_fs_stem(host->content_path, app->game_name, sizeof(app->game_name));
        }
    }
    settings_set_context(&app->settings, app->core_name, app->game_name);
}

// ---------------------------------------------------------------------------
// Recent games
// ---------------------------------------------------------------------------

static void refresh_recent_labels(App* app)
{
    app->recent_labels.clear();
    app->recent_label_pointers.clear();
    const char* no_game = piccolo_text(&app->strings, Str::recent_no_game);
    for (size_t i = 0; i < app->recent.games.size(); i++)
    {
        char label[600] = {};
        recent_games_label(&app->recent.games[i], no_game, label, sizeof(label));
        app->recent_labels.push_back(label);
    }
    // Pointers after all the strings exist (push_back may move them).
    for (size_t i = 0; i < app->recent_labels.size(); i++)
    {
        app->recent_label_pointers.push_back(app->recent_labels[i].c_str());
    }
}

static void remember_recent(App* app, const char* core_path, const char* content_path)
{
    recent_games_add(&app->recent, core_path, content_path);
    if (!app->recent_path.empty())
    {
        recent_games_save(&app->recent, app->recent_path.c_str());
    }
    refresh_recent_labels(app);
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
    bool menu_open = app->menu_visible || app->picker.open || app->settings_window.open || app->settings_window.image_open;
    bool pause_for_menu = menu_open && settings_get_bool(&app->settings, PICCOLO_PAUSE_IN_MENU);
    bool pause_for_focus = !app->window_focused && settings_get_bool(&app->settings, PICCOLO_PAUSE_ON_FOCUS_LOSS);
    bool pause = game_running(app) && (pause_for_menu || pause_for_focus);
    if (pause && !app->session.paused)
    {
        audio_output_clear(&app->session.audio);
    }
    app->session.paused = pause;

    // SDL blocks the screensaver; let it through while nothing plays, if allowed.
    bool allow = settings_get_bool(&app->settings, PICCOLO_SCREENSAVER_WHEN_PAUSED) && (!game_running(app) || pause);
    if (allow != app->screensaver_allowed)
    {
        app->screensaver_allowed = allow;
        if (allow)
        {
            SDL_EnableScreenSaver();
        }
        else
        {
            SDL_DisableScreenSaver();
        }
    }
}

// AUTO latency per core: a game starts its automatic target where this core
// ended last time, and where it goes is recorded as it moves (saved with the
// other state: at each game start and on quit).
static void apply_learned_latency(App* app)
{
    AudioOutputConfig* config = &app->session.audio_config;
    if (!game_running(app) || !config->automatic_latency || app->core_name[0] == '\0')
    {
        app->learned_for_start = 0;   // read again if AUTO comes back
        return;
    }
    if (app->learned_for_start != app->game_starts)
    {
        // This game's start value, read once (the same for the whole game:
        // a new start value restarts the target).
        app->learned_for_start = app->game_starts;
        const char* value = settings_file_get(&app->audio_learned, "", app->core_name);
        long ms = value != nullptr ? std::strtol(value, nullptr, 10) : 0;
        app->learned_start_ms = ms > 0 && ms <= static_cast<long>(RATE_CONTROL_MAX_TARGET_MS) ? static_cast<uint32_t>(ms) : 0;
        app->learned_recorded_ms = app->learned_start_ms;
    }
    if (app->learned_start_ms > 0)
    {
        config->latency_ms = app->learned_start_ms;
    }

    const AudioOutput* audio = &app->session.audio;
    if (audio->stream == nullptr || !audio->rate.automatic)
    {
        return;
    }
    uint32_t now = static_cast<uint32_t>(audio->rate.target_ms + 0.5);
    if (now != app->learned_recorded_ms)
    {
        char number[16] = {};
        std::snprintf(number, sizeof(number), "%u", now);
        settings_file_set(&app->audio_learned, "", app->core_name, number);
        app->learned_recorded_ms = now;
        app->audio_learned_dirty = true;
    }
}

static void save_learned_latency(App* app)
{
    if (app->audio_learned_dirty && !app->audio_learned_path.empty())
    {
        settings_file_save(&app->audio_learned, app->audio_learned_path.c_str(),
                           "piccolo: automatic audio latency learned per core (ms)");
        app->audio_learned_dirty = false;
    }
}

static void save_core_options(App* app)
{
    if (app->option_store.dirty && !app->option_store_path.empty())
    {
        core_options_store_save(&app->option_store, app->option_store_path.c_str());
    }
    // The cache right away: a core that crashes later can still be cleaned.
    if (app->option_cache.dirty && !app->option_cache_path.empty())
    {
        core_options_cache_save(&app->option_cache, app->option_cache_path.c_str());
    }
}

static void start_game(App* app, const FilePickerChoice* choice)
{
    save_core_options(app);   // what was changed for the previous game
    save_learned_latency(app);   // what the previous game learned
    app->game_starts++;
    auto_frame_delay_reset(&app->auto_delay);   // each game finds its own
    bool started = core_session_start(&app->session, &app->shell, choice->core_path, choice->content_path);
    update_settings_context(app);
    if (started)
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
        remember_recent(app, choice->core_path, choice->content_path);
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
                set_status(app, piccolo_text(&app->strings, Str::status_reset));
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
        case MenuAction::recent_game:
            break;   // handled where the menu bar is drawn (it needs the index)
        case MenuAction::general:
            settings_window_open(&app->settings_window, SettingsTab::general);
            break;
        case MenuAction::video:
            settings_window_open(&app->settings_window, SettingsTab::video);
            break;
        case MenuAction::sound:
            settings_window_open(&app->settings_window, SettingsTab::sound);
            break;
        case MenuAction::debug_window:
            app->show_debug_window = !app->show_debug_window;
            break;
        default:
            set_status(app, piccolo_text(&app->strings, Str::status_not_there_yet));
            break;
    }
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

// F11 / Alt+Enter: the global fullscreen setting, saved on exit.
static void toggle_fullscreen(App* app)
{
    bool fullscreen = settings_get_bool(&app->settings, PICCOLO_DISPLAY_FULLSCREEN);
    settings_set_bool(&app->settings, SettingLayer::global, PICCOLO_DISPLAY_FULLSCREEN, !fullscreen);
}

// The running game's picture for the dynamic window mode: the core's base
// size (what it normally shows) with the Picture settings. False when no
// game runs.
static bool game_picture(const App* app, ViewportInput* picture)
{
    if (!game_running(app))
    {
        return false;
    }
    const retro_game_geometry* geometry = &app->session.host->av_info.geometry;
    if (geometry->base_width == 0 || geometry->base_height == 0)
    {
        return false;
    }
    *picture = ViewportInput{};
    picture->frame_width = geometry->base_width;
    picture->frame_height = geometry->base_height;
    picture->core_aspect = static_cast<double>(geometry->aspect_ratio);
    piccolo_settings_viewport(&app->settings, picture);
    return true;
}

// Low latency: in Vsync timing the loop is paced by the display, so waiting
// a few ms here (right after presenting) moves the next input read and core
// frame closer to the next refresh. Only after waiting for present: without
// it the loop blocks in acquire later, after the input read, and a delay here
// only adds latency (the setting is greyed out then).
// FRAME DELAY, or with AUTO FRAME DELAY the delay it is finding or found for
// this game (frame_delay.h). AUTO watches this loop's missed refresh
// (frame_timing) and needs a minute of uninterrupted play to lock: the menu,
// a pause or another window having focus start that minute over.
// This game's section in frame_delay.cfg: "core/game" ("core" without content).
static std::string frame_delay_section(const App* app)
{
    std::string section = app->core_name;
    if (app->game_name[0] != '\0')
    {
        section += "/";
        section += app->game_name;
    }
    return section;
}

static void save_frame_delay_file(App* app)
{
    if (!app->frame_delay_saved_path.empty())
    {
        settings_file_save(&app->frame_delay_saved, app->frame_delay_saved_path.c_str(),
                           "piccolo: automatic frame delay found per game (ms)");
    }
}

// SEARCH AGAIN: forget this game's delay and start the search over.
static void search_frame_delay_again(App* app)
{
    if (app->core_name[0] != '\0' &&
        settings_file_remove(&app->frame_delay_saved, frame_delay_section(app).c_str(), "delay_ms"))
    {
        save_frame_delay_file(app);
    }
    auto_frame_delay_reset(&app->auto_delay);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Frame delay: searching again (automatic)");
}

static void frame_delay(App* app)
{
    bool automatic = settings_get_bool(&app->settings, PICCOLO_LATENCY_FRAME_DELAY_AUTO);
    int32_t manual_ms = settings_get_int(&app->settings, PICCOLO_LATENCY_FRAME_DELAY);
    bool waits = settings_get_bool(&app->settings, PICCOLO_LATENCY_WAIT_FOR_PRESENT);
    app->frame_delay_ms = 0;
    bool applies = waits && game_running(app) && !app->session.paused &&
                   app->session.timing.effective == TimingMode::vsync && (automatic || manual_ms > 0);
    if (!applies)
    {
        app->delay_last_ns = 0;
        auto_frame_delay_interrupt(&app->auto_delay);
        return;
    }
    uint32_t use = static_cast<uint32_t>(manual_ms > 0 ? manual_ms : 0);
    if (automatic)
    {
        bool menu_open = app->menu_visible || app->picker.open || app->settings_window.open ||
                         app->settings_window.image_open;
        if (menu_open || !app->window_focused)
        {
            auto_frame_delay_interrupt(&app->auto_delay);
        }
        uint64_t now = SDL_GetTicksNS();
        double elapsed = app->delay_last_ns > 0 ? static_cast<double>(now - app->delay_last_ns) / 1e9 : 0.0;
        app->delay_last_ns = now;
        if (!app->auto_delay.started && app->core_name[0] != '\0')
        {
            // Found before for this game: locked at it from the start.
            const char* saved = settings_file_get(&app->frame_delay_saved, frame_delay_section(app).c_str(), "delay_ms");
            long ms = saved != nullptr ? std::strtol(saved, nullptr, 10) : -1;
            if (ms >= 0 && ms <= 1000)
            {
                auto_frame_delay_restore(&app->auto_delay, static_cast<uint32_t>(ms));
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Frame delay: %ld ms, found before for this game (automatic)", ms);
            }
        }
        bool started = app->auto_delay.started;
        uint32_t before = app->auto_delay.delay_ms;
        bool was_locked = app->auto_delay.locked;
        use = auto_frame_delay_update(&app->auto_delay, app->session.timing.missed, elapsed,
                                      auto_frame_delay_start_ms(app->session.timing.display_hz));
        if (!started)
        {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Frame delay: starting at %u ms (automatic)", use);
        }
        else if (use != before)
        {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Frame delay: %u -> %u ms (automatic)", before, use);
        }
        if (app->auto_delay.locked && !was_locked)
        {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Frame delay: locked at %u ms", use);
            if (app->core_name[0] != '\0')
            {
                char number[16] = {};
                std::snprintf(number, sizeof(number), "%u", use);
                settings_file_set(&app->frame_delay_saved, frame_delay_section(app).c_str(), "delay_ms", number);
                save_frame_delay_file(app);
            }
        }
    }
    app->frame_delay_ms = use;
    if (use > 0)
    {
        SDL_DelayPrecise(static_cast<Uint64>(use) * 1000000u);
    }
}

// The language setting -> the UI text and what cores are told (GET_LANGUAGE,
// from the next core start). Only redone when the setting changes: "auto"
// asks the system, which allocates.
static void update_language(App* app)
{
    uint32_t setting = settings_get_enum(&app->settings, PICCOLO_LANGUAGE);
    if (setting == app->language_setting)
    {
        return;
    }
    app->language_setting = setting;
    uint32_t language = piccolo_settings_language(&app->settings);
    app->session.language = piccolo_language_retro(language);
    if (language == 0)
    {
        localization_use_english(&app->strings);
        refresh_recent_labels(app);
        return;
    }

    // lang/<code>.po: the user's copy in piccolo's folder wins over the one
    // that comes with piccolo (next to the program).
    std::string file = std::string("lang/") + piccolo_language_code(language) + ".po";
    std::string user_copy = app->paths.root.empty() ? std::string() : app->paths.root + "/" + file;
    const char* base = SDL_GetBasePath();   // ends with a separator; SDL owns it
    std::string bundled = base != nullptr ? std::string(base) + file : file;
    const std::string* path = !user_copy.empty() && host_fs_exists(user_copy.c_str()) ? &user_copy : &bundled;
    localization_load_po(&app->strings, path->c_str(), piccolo_language_code(language));
    refresh_recent_labels(app);
}

static void update_window(App* app)
{
    ViewportInput picture = {};
    bool has_picture = game_picture(app, &picture);
    piccolo_window_apply(&app->window, app->shell.window, &app->settings, has_picture ? &picture : nullptr);
}

// Keyboard as the game's joypad only while the game has the screen.
static bool game_has_keyboard(const App* app)
{
    return game_running(app) && !app->menu_visible && !app->picker.open && !app->settings_window.open &&
           !app->settings_window.image_open;
}

static void refresh_audio_devices(App* app)
{
    audio_output_list_devices(&app->audio_devices);
    app->audio_device_pointers.clear();
    for (size_t i = 0; i < app->audio_devices.size(); i++)
    {
        app->audio_device_pointers.push_back(app->audio_devices[i].c_str());
    }
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

        if ((event.type == SDL_EVENT_AUDIO_DEVICE_ADDED || event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED) &&
            !event.adevice.recording)
        {
            refresh_audio_devices(app);
        }
        if (event.type == SDL_EVENT_QUIT)
        {
            app->running = false;
        }
        if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED || event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
        {
            app->window_focused = event.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
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
                    // Close the topmost window first (IMAGE SETTINGS, SETTINGS,
                    // LOAD GAME); otherwise show/hide the menu (it stays up when
                    // nothing runs).
                    if (app->settings_window.image_open)
                    {
                        app->settings_window.image_open = false;
                    }
                    else if (app->settings_window.open)
                    {
                        app->settings_window.open = false;
                    }
                    else if (app->picker.open)
                    {
                        app->picker.open = false;
                    }
                    else if (game_running(app))
                    {
                        app->menu_visible = !app->menu_visible;
                    }
                    break;
                case SDLK_F11:
                    toggle_fullscreen(app);
                    break;
                case SDLK_RETURN:
                    if (event.key.mod & SDL_KMOD_ALT)
                    {
                        toggle_fullscreen(app);
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
    const ImGuiIO& io = ImGui::GetIO();
    ViewportInput input = {};
    input.frame_width = session->texture.content_width;
    input.frame_height = session->texture.content_height;
    input.core_aspect = static_cast<double>(session->host->av_info.geometry.aspect_ratio);
    input.window_width = static_cast<uint32_t>(io.DisplaySize.x);
    input.window_height = static_cast<uint32_t>(io.DisplaySize.y);
    piccolo_settings_viewport(&app->settings, &input);

    ViewportResult viewport = {};
    if (viewport_compute(&input, &viewport) &&
        picture_prepare(&app->picture, &app->shell.vulkan, &session->texture, &viewport))
    {
        // The background draw list is behind every window, menu bar included.
        picture_draw(&app->picture, ImGui::GetBackgroundDrawList(), &session->texture, session->texture_id, 0.0f, 0.0f);
    }
}

static void draw_settings(App* app)
{
    const CoreSession* session = &app->session;
    ImageWindowSource image = {};
    if (game_running(app) && session->texture.has_content && session->texture_id != 0)
    {
        image.texture_id = session->texture_id;
        streaming_texture_uv_max(&session->texture, &image.uv_max_x, &image.uv_max_y);
        image.frame_width = session->texture.content_width;
        image.frame_height = session->texture.content_height;
    }
    SettingsWindowContext context = {};
    context.settings = &app->settings;
    context.paths = &app->paths;
    context.core_name = app->core_name;
    context.game_name = app->game_name;
    context.image = &image;
    context.strings = &app->strings;
    // Text with numbers: the translated part, then the numbers (no formats in translations).
    const char* window_label = piccolo_text(&app->strings, Str::section_window);
    char window_size[200] = {};
    if (app->window.target_width == 0)
    {
        std::snprintf(window_size, sizeof(window_size), "%s: %s", window_label,
                      piccolo_text(&app->strings, Str::window_sized_when_running));
    }
    else if (app->window.target_scale > 0)
    {
        std::snprintf(window_size, sizeof(window_size), "%s: %u X %u (X%u)", window_label, app->window.target_width,
                      app->window.target_height, app->window.target_scale);
    }
    else
    {
        std::snprintf(window_size, sizeof(window_size), "%s: %u X %u", window_label, app->window.target_width,
                      app->window.target_height);
    }
    context.window_size = window_size;
    context.present_wait = piccolo_text(&app->strings, app->shell.vulkan.present_wait_version > 0
                                                           ? Str::present_wait_screen
                                                           : Str::present_wait_gpu);
    context.audio_devices = app->audio_device_pointers.data();
    context.audio_device_count = static_cast<uint32_t>(app->audio_device_pointers.size());
    char audio_latency[200] = {};
    const AudioOutput* audio = &app->session.audio;
    if (audio->stream == nullptr)
    {
        std::snprintf(audio_latency, sizeof(audio_latency), "%s",
                      piccolo_text(&app->strings, Str::audio_latency_no_game));
    }
    else
    {
        std::snprintf(audio_latency, sizeof(audio_latency), "%s: %.0f MS%s, %s: %u MS",
                      piccolo_text(&app->strings, Str::audio_latency_now), audio->rate.target_ms,
                      audio->rate.automatic ? " (AUTO)" : "", piccolo_text(&app->strings, Str::audio_latency_minimum),
                      audio->rate.floor_ms);
    }
    context.audio_latency = audio_latency;
    Str game_mode = Str::game_mode_off;
    switch (app->game_mode.state)
    {
    case GameModeState::off:
        game_mode = Str::game_mode_off;
        break;
    case GameModeState::active:
        game_mode = Str::game_mode_active;
        break;
    case GameModeState::not_available:
        game_mode = Str::game_mode_not_available;
        break;
    case GameModeState::not_supported:
        game_mode = Str::game_mode_not_supported;
        break;
    }
    context.game_mode = piccolo_text(&app->strings, game_mode);
    char frame_delay_text[200] = {};
    const AutoFrameDelay* auto_delay = &app->auto_delay;
    if (game_running(app) && app->session.timing.effective != TimingMode::vsync)
    {
        std::snprintf(frame_delay_text, sizeof(frame_delay_text), "%s",
                      piccolo_text(&app->strings, Str::frame_delay_needs_vsync));
    }
    else if (!game_running(app) || !auto_delay->started)
    {
        std::snprintf(frame_delay_text, sizeof(frame_delay_text), "%s",
                      piccolo_text(&app->strings, Str::frame_delay_needs_game));
    }
    else if (auto_delay->locked)
    {
        // While paused (this window pauses the game) the delay isn't applied:
        // what's shown is what resuming uses.
        std::snprintf(frame_delay_text, sizeof(frame_delay_text), "%s: %u MS - %s",
                      piccolo_text(&app->strings, Str::frame_delay_now), auto_delay->delay_ms,
                      piccolo_text(&app->strings, Str::frame_delay_locked));
    }
    else
    {
        std::snprintf(frame_delay_text, sizeof(frame_delay_text), "%s: %u MS - %s %.0f S",
                      piccolo_text(&app->strings, Str::frame_delay_now), auto_delay->delay_ms,
                      piccolo_text(&app->strings, Str::frame_delay_searching),
                      auto_frame_delay_seconds_to_lock(auto_delay));
    }
    context.frame_delay = frame_delay_text;
    context.frame_delay_found = game_running(app) && auto_delay->locked;
    context.core_options.session = &app->session;
    context.core_options.store = &app->option_store;
    context.core_options.cache = &app->option_cache;
    context.core_options.running_core = game_running(app) ? app->core_name : "";
    context.core_options.game = app->game_name;
    const std::string& folder_section = app->option_store.directory_section;
    size_t folder_at = folder_section.find("/dir:");
    context.core_options.folder = folder_at != std::string::npos ? folder_section.c_str() + folder_at + 5 : "";
    settings_window_draw(&app->settings_window, &context, app->pixel_font, app->ui_scale);
    if (app->settings_window.search_frame_delay)
    {
        app->settings_window.search_frame_delay = false;
        search_frame_delay_again(app);
    }

    // Closed (by its button or Esc): apply the folders and save.
    bool open = app->settings_window.open || app->settings_window.image_open;
    if (app->settings_window_was_open && !open)
    {
        apply_folders(app);
        save_settings(app);
        save_core_options(app);
    }
    app->settings_window_was_open = open;
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
        ImGui::Text("Audio: x%.5f, %u ms queued (target %.0f ms%s), %llu underruns", session->audio.current_ratio,
                    audio_output_queued_ms(&session->audio), session->audio.rate.target_ms,
                    session->audio.rate.automatic ? ", auto" : "", static_cast<unsigned long long>(session->audio.underruns));
        ImGui::Text("Frames: %llu missed refreshes, %llu caught up; frame delay %u ms",
                    static_cast<unsigned long long>(session->timing.missed_total),
                    static_cast<unsigned long long>(session->timing.caught_up_total), app->frame_delay_ms);
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

    if (!settings_init(&app->settings, piccolo_settings_table(), piccolo_settings_count()))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "piccolo's settings table is broken (see above)");
    }
    if (!localization_init(&app->strings, piccolo_strings_table(), text_id(Str::count)))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "piccolo's strings table is broken (see above)");
    }

    // Everything in piccolo's folder, migrated once from ~/.host-libretro.
    // Without it nothing is saved, but games still run.
    ImGuiIO& io = ImGui::GetIO();
    if (app_paths_init(&app->paths, "piccolo", "piccolo.cfg"))
    {
        app->imgui_ini = app->paths.settings + "/imgui.ini";
        io.IniFilename = app->imgui_ini.c_str();
        app->settings_path = app->paths.settings + "/piccolo.cfg";
        settings_load(&app->settings, app->settings_path.c_str());
        apply_folders(app);
        load_last_used(app);
        app->recent_path = app->paths.settings + "/recent.cfg";
        recent_games_load(&app->recent, app->recent_path.c_str());
        app->audio_learned_path = app->paths.settings + "/audio_latency.cfg";
        settings_file_load(&app->audio_learned, app->audio_learned_path.c_str());
        app->option_store_path = app->paths.settings + "/core_options.cfg";
        app->option_cache_path = app->paths.settings + "/core_options_cache.cfg";
        core_options_store_load(&app->option_store, app->option_store_path.c_str());
        core_options_cache_load(&app->option_cache, app->option_cache_path.c_str());
        app->session.option_store = &app->option_store;
        app->session.option_cache = &app->option_cache;
        app->frame_delay_saved_path = app->paths.settings + "/frame_delay.cfg";
        settings_file_load(&app->frame_delay_saved, app->frame_delay_saved_path.c_str());
        piccolo_window_restore(&app->window, app->shell.window, &app->settings);
    }
    else
    {
        io.IniFilename = nullptr;
    }
    update_language(app);
    set_status(app, piccolo_text(&app->strings, Str::status_start));

    // SDL's audio subsystem from the start (no device is opened until a core
    // runs): the Sound tab lists the devices and follows hotplug.
    app->audio_ready = SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (!app->audio_ready)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Audio subsystem: %s", SDL_GetError());
    }
    refresh_audio_devices(app);

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
        app->session.timing.mode = piccolo_settings_timing(&app->settings);
        app->session.timing.catch_up = settings_get_bool(&app->settings, PICCOLO_VIDEO_CATCH_UP);
        update_window(app);
        update_language(app);
        renderer_set_frames_in_flight(&app->shell.renderer, &app->shell.vulkan,
                                      piccolo_settings_frames_in_flight(&app->settings));
        app->shell.renderer.wait_for_present = settings_get_bool(&app->settings, PICCOLO_LATENCY_WAIT_FOR_PRESENT);
        piccolo_settings_audio(&app->settings, &app->session.audio_config);
        apply_learned_latency(app);
        // Held for the whole game, paused or not: a request is a D-Bus call
        // that may block, so not on every pause and resume.
        game_mode_set(&app->game_mode, game_running(app) && settings_get_bool(&app->settings, PICCOLO_GAME_MODE));
        core_session_update(&app->session, &app->shell);
        if (app->option_cache.dirty)
        {
            save_core_options(app);   // a core declared new options: on disk before it can crash
        }
        if (core_session_run_frame(&app->session, &app->shell, nullptr, game_has_keyboard(app)) == CoreSessionFrame::shut_down)
        {
            set_status(app, app->session.message);
            app->menu_visible = true;
            update_settings_context(app);
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
                int32_t recent_picked = -1;
                MenuAction action = retro_ui_menu_bar(app->pixel_font, app->ui_scale, &app->strings,
                                                      app->recent_label_pointers.data(),
                                                      static_cast<int32_t>(app->recent_label_pointers.size()),
                                                      &recent_picked);
                if (action == MenuAction::recent_game && recent_picked >= 0)
                {
                    // Started at the top of the next loop, like LOAD GAME (see pending_start).
                    const RecentGame* game = &app->recent.games[static_cast<size_t>(recent_picked)];
                    app->pending_start = FilePickerChoice{};
                    std::snprintf(app->pending_start.core_path, sizeof(app->pending_start.core_path), "%s",
                                  game->core_path.c_str());
                    std::snprintf(app->pending_start.content_path, sizeof(app->pending_start.content_path), "%s",
                                  game->content_path.c_str());
                    app->has_pending_start = true;
                }
                handle_menu_action(app, action);
            }

            if (file_picker_draw(&app->picker, &app->catalog, app->pixel_font, app->ui_scale, &app->strings,
                                 &app->pending_start))
            {
                app->has_pending_start = true;   // see pending_start
            }
            draw_settings(app);
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
            frame_delay(app);
        }
        core_session_wait(&app->session);
    }

    piccolo_window_capture(&app->window, app->shell.window);
    save_last_used(app);
    save_settings(app);
    save_learned_latency(app);
    save_core_options(app);
    core_session_destroy(&app->session, &app->shell);   // writes save RAM
    game_mode_set(&app->game_mode, false);
    if (app->audio_ready)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    picture_destroy(&app->picture, &app->shell.vulkan);
    app_shell_destroy(&app->shell);
    delete app;
    return 0;
}
