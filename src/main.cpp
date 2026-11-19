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

#include "app_icon.h"
#include "app_paths.h"
#include "app_shell.h"
#include "frame_delay.h"
#include "game_mode.h"
#include "host_libretro_version.h"
#include "picture.h"
#include "piccolo_version.h"
#include "piccolo_settings.h"
#include "piccolo_text.h"
#include "piccolo_window.h"
#include "quick_menu.h"
#include "recent_games.h"
#include "safe_start_window.h"
#include "settings_window.h"
#include "host_fs.h"
#include "settings_file.h"
#include "core_catalog.h"
#include "content_cache.h"
#include "core_session.h"
#include "debug_ui.h"
#include "input_bindings.h"
#include "input_config.h"
#include "input_sdl.h"
#include "input_window.h"
#include "map_window.h"
#include "input_net.h"
#include "net_access.h"
#include "net_games.h"
#include "net_remote.h"
#include "net_trust.h"
#include "net_window.h"
#include "file_picker.h"
#include "pixel_font.h"
#include "pixel_font_imgui.h"
#include "retro_ui.h"
#include "vk_swapchain.h"

// Files extracted from zip / 7z archives (content_cache.h) are kept up to
// this much: room for several disc images, small next to what the games
// themselves take. Trimmed after each game starts.
constexpr uint64_t PICCOLO_CONTENT_CACHE_MAX_BYTES = 4ull * 1024ull * 1024ull * 1024ull;


struct App
{
    AppShell shell = {};
    InputSystem input = {};       // controllers, ports, hotkeys (input_bindings.h)
    InputSdl    input_sdl = {};   // the keyboard and gamepads, from SDL
    bool        ui_keys_down[5] = {};   // gamepad interface actions given to ImGui as keys (drive_ui_with_gamepads)
    bool        ui_back_down = false;
    uint64_t    ui_back_since_ms = 0;      // BACK / SELECT held since (0 = not held)
    bool        ui_back_done = false;      // this hold already closed a window
    bool        open_first_menu = false;
    std::string input_config_path;   // settings/input.cfg
    InputWindow input_window = {};   // CONFIG > INPUT
    MapWindow   map_window = {};     // MAP CONTROLLER
    InputLayouts layouts = {};       // controller layouts (layouts/*.cfg)
    std::vector<PiccoloThemeFile> themes;   // interface themes from files (themes/*.cfg)
    std::vector<uint32_t> map_offered;   // joysticks offered MAP CONTROLLER this session

    // HOLD START TO REMAP: a controller plays a core's device it hasn't
    // played before (no decided InputCoreProfile).
    bool     remap_prompt = false;
    uint32_t remap_prompt_controller = 0;
    uint32_t remap_prompt_port = 0;
    uint64_t remap_prompt_started_ms = 0;
    uint64_t remap_prompt_start_held_ms = 0;   // when START began being held (0 = not held)   // OPEN MENU pressed: the menu bar opens its first menu this frame
    bool     running = false;
    bool     show_debug_window = false;
    bool     show_imgui_demo = false;

    ImFont* pixel_font = nullptr;   // piccolo's pixel font as an ImGui font
    int32_t ui_scale   = 1;         // window pixels per retro pixel
    char    status[256] = "";   // set at startup, in the chosen language

    CoreSession session = {};
    CoreCatalog catalog = {};

    // Network (CONFIG > NETWORK): the listener with access control and
    // pairing, while net_enabled. The device key and the paired devices live
    // in <settings>/net_identity.cfg and net_devices.cfg.
    bool       net_ready = false;        // net_init() succeeded
    bool       net_running = false;      // `net` is listening
    bool       net_listen_failed = false;
    bool       net_has_identity = false;
    NetKeypair net_identity = {};
    char       net_fingerprint[32] = "";
    uint16_t   net_port = 0;             // what `net` listens on
    NetAccess  net = {};
    InputNet   input_net = {};           // paired devices' pads, in the input router
    NetGames   net_games = {};           // games from devices with FULL access (net_games.h)
    bool       net_load_waiting = false; // a load_request is in pending_start: answered once it ran
    uint32_t   net_load_peer = NET_NO_PEER;
    uint32_t   net_load_id = 0;
    NetRemote  net_remote = {};          // settings and actions from devices (net_remote.h)
    bool       net_remote_ready = false;
    bool       net_paused = false;       // PAUSE from a device
    uint32_t   net_paused_by = NET_NO_PEER;   // which: its leaving (or another game) resumes
    char       net_peer_language[NET_MAX_PEERS][8] = {};   // what each device asked for ("" = English)
    bool       net_fast_forward[NET_MAX_PEERS] = {};   // FAST FORWARD held, per peer
    NetRemoteGame net_actions_sent;      // what the action lists last said (sent again when it changes)
    // What the devices were last told runs (game_state): sent again when it changes.
    std::string net_sent_core;           // "" = nothing runs
    std::string net_sent_content;
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
    // The part of the core's frame on screen (a cursor maps from it to the
    // whole frame), and the ports' cursors for the core (mouse, pointer, lightgun).
    uint32_t           view_source[4] = {};      // x, y, width, height in core pixels
    uint32_t           view_frame[2] = {};       // the frame's width, height
    LibretroInputState cursor_input = {};
    bool               mouse_grabbed = false;
    bool               mouse_released = false;   // MOUSE GRAB let it go

    // Settings (piccolo_settings.h) in <settings>/piccolo.cfg, and their window.
    Settings       settings = {};
    std::string    settings_path;
    SettingsWindow settings_window = {};
    const char*    choose_folder_key = nullptr;   // the folder setting CHOOSE FOLDER sets
    bool           settings_window_was_open = false;
    bool           options_window_was_open = false;   // the quick menu or the options manager
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

    // Safe start (safe_start_window.h): settings/running.cfg says a game
    // session is in progress; found at launch, that session didn't end.
    std::string      running_marker_path;
    bool             marker_written = false;          // this run wrote it (a session is on)
    bool             marker_options_changed = false;  // and recorded an option change in it
    std::string      marker_core_path;
    std::string      marker_content_path;
    std::string      crashed_core;                    // short name of the core whose session didn't end
    bool             crashed_after_options = false;
    bool             safe_start_open = false;
    bool             safe_start_confirmed = false;    // the next start skips the check (answered)
    FilePickerChoice safe_start_pending = {};

    // The core options UI's state, shared by the quick menu (live options)
    // and the CORE OPTIONS MANAGER window (CONFIG menu).
    CoreOptionsUiState core_options_state;
    bool               core_options_manager = false;   // the window is open
    bool               core_options_manager_was_open = false;

    QuickMenu quick_menu;              // F1 while a game runs
    bool      pending_close_game = false;   // CLOSE GAME: done at the top of the next loop (like starts)

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
    std::snprintf(app->session.cache_directory, sizeof(app->session.cache_directory), "%s", app->paths.cache.c_str());

    std::string cores = folder_setting(app, PICCOLO_PATH_CORES, app->paths.cores);
    if (cores != app->cores_directory)
    {
        app->cores_directory = cores;
        std::string cache_path = app->paths.root + "/core_info_cache.cfg";
        core_catalog_init(&app->catalog, cores.c_str(), cache_path.c_str());
        core_catalog_scan(&app->catalog, nullptr);
    }
}

// The theme's text shadows, added to what ImGui is about to draw.
static void add_text_shadows(ImDrawData* draw_data, void* user)
{
    const App* app = static_cast<const App*>(user);
    retro_ui_text_shadows(draw_data, app->ui_scale);
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
    bool menu_open = app->menu_visible || app->picker.open || app->settings_window.open ||
                     app->settings_window.image_open || app->quick_menu.open || app->core_options_manager ||
                     app->input_window.open || map_window_open_now(&app->map_window);
    bool pause_for_menu = (menu_open && settings_get_bool(&app->settings, PICCOLO_PAUSE_IN_MENU)) || app->remap_prompt;
    bool pause_for_focus = !app->window_focused && settings_get_bool(&app->settings, PICCOLO_PAUSE_ON_FOCUS_LOSS);
    bool pause = game_running(app) && (pause_for_menu || pause_for_focus || app->net_paused);
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

// settings/running.cfg: a game session is on (written before the core
// loads, so a crash while loading is noticed too).
static void write_running_marker(App* app)
{
    if (app->running_marker_path.empty())
    {
        return;
    }
    SettingsFile file = {};
    settings_file_set(&file, "", "core", app->marker_core_path.c_str());
    settings_file_set(&file, "", "content", app->marker_content_path.c_str());
    settings_file_set(&file, "", "options_changed", app->marker_options_changed ? "true" : "false");
    app->marker_written = settings_file_save(&file, app->running_marker_path.c_str(),
                                             "piccolo: a game is running (removed when it ends properly)");
}

// The session ended properly.
static void remove_running_marker(App* app)
{
    if (app->marker_written)
    {
        SDL_RemovePath(app->running_marker_path.c_str());
        app->marker_written = false;
    }
    app->marker_options_changed = false;
}

// At launch: a marker left behind means the last session didn't end.
static void check_running_marker(App* app)
{
    SettingsFile file = {};
    if (app->running_marker_path.empty() || !host_fs_exists(app->running_marker_path.c_str()) ||
        !settings_file_load(&file, app->running_marker_path.c_str()))
    {
        return;
    }
    const char* core_path = settings_file_get(&file, "", "core");
    const char* changed = settings_file_get(&file, "", "options_changed");
    char core[256] = {};
    if (core_path != nullptr && libretro_core_short_name(core_path, core, sizeof(core)))
    {
        app->crashed_core = core;
        app->crashed_after_options = changed != nullptr && std::strcmp(changed, "true") == 0;
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "The last session with %s didn't end properly%s", core,
                    app->crashed_after_options ? " (its options had been changed)" : "");
    }
}

// Ask before starting the core whose last session didn't end, when it has
// stored options (with none there's nothing to choose).
static bool needs_safe_start(App* app, const FilePickerChoice* choice)
{
    char core[256] = {};
    if (app->crashed_core.empty() || !libretro_core_short_name(choice->core_path, core, sizeof(core)) ||
        app->crashed_core != core)
    {
        return false;
    }
    std::vector<std::string> sections;
    core_options_store_sections_of(&app->option_store, core, &sections);
    if (sections.empty())
    {
        app->crashed_core.clear();
        return false;
    }
    return true;
}

// True if the game started (false also when it waits for SAFE START).
static bool start_game(App* app, const FilePickerChoice* choice)
{
    if (!app->safe_start_confirmed && needs_safe_start(app, choice))
    {
        app->safe_start_pending = *choice;
        app->safe_start_open = true;   // started (or not) when it's answered
        return false;
    }
    app->safe_start_confirmed = false;
    remove_running_marker(app);   // the previous game, if any, ended properly
    save_core_options(app);   // what was changed for the previous game
    save_learned_latency(app);   // what the previous game learned
    app->game_starts++;
    auto_frame_delay_reset(&app->auto_delay);   // each game finds its own
    app->marker_core_path = choice->core_path;
    app->marker_content_path = choice->content_path;
    write_running_marker(app);
    bool started = core_session_start(&app->session, &app->shell, choice->core_path, choice->content_path);
    update_settings_context(app);
    if (started)
    {
        // The emulated device on each port, as kept for this core (or game).
        for (unsigned port = 0; port < LIBRETRO_MAX_PORTS; port++)
        {
            int32_t device = settings_get_int(&app->settings, PICCOLO_INPUT_PORT_DEVICES[port]);
            if (device >= 0 && static_cast<unsigned>(device) != app->session.host->port_devices[port])
            {
                libretro_host_set_port_device(app->session.host, port, static_cast<unsigned>(device));
            }
        }
    }
    if (!started)
    {
        remove_running_marker(app);   // failed cleanly: not a crash
    }
    if (started)
    {
        app->crashed_core.clear();   // answered (or not this core's turn): it runs again
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
        // Files extracted from archives: keep the cache bounded, never
        // touching the game that just started.
        content_cache_trim(app->session.cache_directory, PICCOLO_CONTENT_CACHE_MAX_BYTES,
                           app->session.host->content_path);
    }
    set_status(app, app->session.message);
    return started;
}

// Saves the session's slot with a thumbnail of the picture as shown: the
// Picture settings' crop and aspect, at x1 (core_session.h).
static bool save_state_with_thumbnail(App* app)
{
    StateThumbnail thumbnail = {};
    const LibretroVideoFrame* video = &app->session.host->video;
    if (game_running(app) && video->width > 0 && video->height > 0)
    {
        ViewportInput input = {};
        input.frame_width = video->width;
        input.frame_height = video->height;
        input.core_aspect = static_cast<double>(app->session.host->av_info.geometry.aspect_ratio);
        piccolo_settings_viewport(&app->settings, &input);
        thumbnail.crop = input.crop;
        if (!viewport_picture_size(&input, 1, &thumbnail.width, &thumbnail.height))
        {
            thumbnail.width = 0;
        }
    }
    bool saved = core_session_save_state(&app->session, &thumbnail);
    quick_menu_refresh(&app->quick_menu);
    return saved;
}

// What the core options UI (quick menu, manager) needs this frame.
static CoreOptionsUiContext core_options_context(App* app)
{
    CoreOptionsUiContext context = {};
    context.strings = &app->strings;
    context.session = &app->session;
    context.store = &app->option_store;
    context.cache = &app->option_cache;
    context.running_core = game_running(app) ? app->core_name : "";
    context.game = app->game_name;
    const std::string& folder_section = app->option_store.directory_section;
    size_t folder_at = folder_section.find("/dir:");
    context.folder = folder_at != std::string::npos ? folder_section.c_str() + folder_at + 5 : "";
    return context;
}

static void handle_menu_action(App* app, MenuAction action)
{
    switch (action)
    {
        case MenuAction::none:
            return;
        case MenuAction::load_game:
        {
            core_catalog_scan(&app->catalog, game_running(app) ? app->session.host : nullptr);
            std::vector<std::string> game_folders;
            piccolo_settings_game_folders(&app->settings, &game_folders);
            file_picker_open(&app->picker, &app->catalog, app->last_folder, app->last_core, &game_folders);
            break;
        }
        case MenuAction::reset:
            if (game_running(app))
            {
                libretro_host_reset(app->session.host);
                set_status(app, piccolo_text(&app->strings, Str::status_reset));
                app->menu_visible = false;
            }
            break;
        case MenuAction::save_state:
            if (save_state_with_thumbnail(app))
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
        case MenuAction::input:
            input_window_open(&app->input_window);
            break;
        case MenuAction::interface:
            settings_window_open(&app->settings_window, SettingsTab::interface);
            break;
        case MenuAction::video:
            settings_window_open(&app->settings_window, SettingsTab::video);
            break;
        case MenuAction::sound:
            settings_window_open(&app->settings_window, SettingsTab::sound);
            break;
        case MenuAction::options_manager:
            core_options_manager_open(&app->core_options_manager, &app->core_options_state);
            break;
        case MenuAction::network:
            settings_window_open(&app->settings_window, SettingsTab::network);
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
// a pause, fast forward or another window having focus start that minute over.
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
    // Not while fast-forwarding: the delay would eat the time fast forward runs
    // extra frames in, and those loops say nothing about this game's pacing
    // (AUTO's minute starts over, like a pause; the delay comes back after).
    bool applies = waits && game_running(app) && !app->session.paused &&
                   !core_session_fast_forwarding(&app->session) &&
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
    piccolo_text_load_language(&app->strings, language, app->paths.root.c_str());
    refresh_recent_labels(app);
}

static void update_window(App* app)
{
    ViewportInput picture = {};
    bool has_picture = game_picture(app, &picture);
    piccolo_window_apply(&app->window, app->shell.window, &app->settings, has_picture ? &picture : nullptr);
}

// Keyboard as the game's joypad only while the game has the screen.

static void refresh_audio_devices(App* app)
{
    audio_output_list_devices(&app->audio_devices);
    app->audio_device_pointers.clear();
    for (size_t i = 0; i < app->audio_devices.size(); i++)
    {
        app->audio_device_pointers.push_back(app->audio_devices[i].c_str());
    }
}

// A window is open over the menu bar (or the game).
static bool any_window_open(const App* app)
{
    return app->picker.open || app->settings_window.open || app->settings_window.image_open || app->quick_menu.open ||
           app->core_options_manager || app->safe_start_open || app->show_debug_window || app->input_window.open ||
           map_window_open_now(&app->map_window);
}

// A menu or a dialog is up: the controllers drive it, not the game.
static bool ui_is_up(const App* app)
{
    return app->menu_visible || !game_running(app) || any_window_open(app) || app->remap_prompt;
}

// The game has the keyboard: its keys go to the core (and are the RetroPad).
static bool game_has_keyboard(const App* app)
{
    return !ui_is_up(app);
}

// Escape / BACK: close the topmost window first (IMAGE SETTINGS, SETTINGS,
// LOAD GAME...); otherwise show/hide the menu (it stays up when nothing runs).
static void close_topmost_or_toggle_menu(App* app)
{
    {
        if (map_window_open_now(&app->map_window))
        {
            map_window_close(&app->map_window, &app->input);
        }
        else if (app->quick_menu.open)
        {
            app->quick_menu.open = false;
        }
        else if (app->core_options_manager)
        {
            app->core_options_manager = false;
        }
        else if (app->input_window.open)
        {
            if (!app->input_window.capturing)   // Escape cancels a SET instead
            {
                app->input_window.open = false;
            }
        }
        else if (app->picker.open && app->picker.mode == FilePickerMode::folder)
        {
            app->picker.open = false;   // CHOOSE FOLDER sits on top of SETTINGS: it goes first
        }
        else if (app->settings_window.image_open)
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
    }
}

// The hotkeys of this frame (input_bindings: keyboard F1 / F2 / F4 / Escape,
// or HOTKEY ENABLE + a button on a gamepad).
static void app_handle_hotkeys(App* app, const InputFrame* frame)
{
    if (frame->pressed[static_cast<uint32_t>(InputAction::hotkey_menu)])
    {
        close_topmost_or_toggle_menu(app);
    }
    if (frame->pressed[static_cast<uint32_t>(InputAction::hotkey_quick_menu)])
    {
        if (app->quick_menu.open)
        {
            app->quick_menu.open = false;
        }
        else if (game_running(app))
        {
            quick_menu_open(&app->quick_menu, app->session.state_slot);
            app->core_options_state.choose_layer = true;   // SAVE TO: the most specific layer in use
        }
    }
    if (frame->pressed[static_cast<uint32_t>(InputAction::hotkey_save_state)])
    {
        handle_menu_action(app, MenuAction::save_state);
    }
    if (frame->pressed[static_cast<uint32_t>(InputAction::hotkey_load_state)])
    {
        handle_menu_action(app, MenuAction::load_state);
    }
}

// Gamepads drive the menus and dialogs as the keyboard does: their
// interface actions become ImGui's arrow keys and Space (activate); BACK
// closes an open list or popup, otherwise the topmost window (as Escape).
struct UiKey
{
    InputAction action;
    ImGuiKey    key;
};

static const UiKey UI_KEYS[] = {
    {InputAction::ui_up, ImGuiKey_UpArrow},
    {InputAction::ui_down, ImGuiKey_DownArrow},
    {InputAction::ui_left, ImGuiKey_LeftArrow},
    {InputAction::ui_right, ImGuiKey_RightArrow},
    {InputAction::ui_accept, ImGuiKey_Space},
};
constexpr size_t UI_KEY_COUNT = sizeof(UI_KEYS) / sizeof(UI_KEYS[0]);

constexpr uint64_t UI_BACK_HOLD_MS = 600;   // hold BACK (or SELECT alone) this long to close a window

static void drive_ui_with_gamepads(App* app, const InputFrame* source_frame, bool ui_up)
{
    ImGuiIO& io = ImGui::GetIO();
    const InputFrame* frame = source_frame;
    for (size_t i = 0; i < UI_KEY_COUNT; i++)
    {
        uint32_t action = static_cast<uint32_t>(UI_KEYS[i].action);
        bool down = ui_up && frame->gamepad_held[action];
        if (down != app->ui_keys_down[i])
        {
            io.AddKeyEvent(UI_KEYS[i].key, down);
            app->ui_keys_down[i] = down;
        }
    }
    // BACK (RetroPad B): a tap closes an open list or popup, or backs out of
    // a view inside a window; a window closes only when BACK, or SELECT by
    // itself, is held (a tap must not close a window by accident).
    uint32_t back = static_cast<uint32_t>(InputAction::ui_back);
    bool back_down = ui_up && frame->gamepad_held[back];
    bool hold_down = ui_up && (back_down || frame->gamepad_enable_alone);
    uint64_t now = SDL_GetTicks();
    if (back_down && !app->ui_back_down)
    {
        if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
        {
            io.AddKeyEvent(ImGuiKey_Escape, true);   // a list or popup closes first
            io.AddKeyEvent(ImGuiKey_Escape, false);
        }
        else if (app->quick_menu.open && app->quick_menu.remap && !app->quick_menu.capturing)
        {
            app->quick_menu.remap = false;   // a view inside a window: back out of it
        }
    }
    if (!hold_down)
    {
        app->ui_back_since_ms = 0;
        app->ui_back_done = false;
    }
    else if (app->ui_back_since_ms == 0)
    {
        app->ui_back_since_ms = now;
    }
    else if (!app->ui_back_done && now - app->ui_back_since_ms >= UI_BACK_HOLD_MS)
    {
        app->ui_back_done = true;   // once per hold
        close_topmost_or_toggle_menu(app);
    }
    app->ui_back_down = back_down;

    // OPEN MENU (START on a gamepad): the menu bar's first menu, when the bar
    // is up and nothing else is open on top of it.
    // Not for the START that just joined a port.
    bool bar_alone = (app->menu_visible || !game_running(app)) && !any_window_open(app) &&
                     !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
    if (bar_alone && frame->joined_port == INPUT_NO_PORT && frame->pressed[static_cast<uint32_t>(InputAction::ui_open_menu)])
    {
        app->open_first_menu = true;
    }
}

// HOLD START TO REMAP: the first controller on a port playing a core's
// device it hasn't played before. START held 1 s opens the remap in the
// quick menu; 5 s without it keeps the RetroPad and remembers that.
constexpr uint64_t REMAP_PROMPT_MS = 5000;
constexpr uint64_t REMAP_HOLD_MS = 1000;

static void update_remap_prompt(App* app, uint64_t now_ms)
{
    InputSystem* input = &app->input;
    if (!game_running(app) || input->core_name.empty())
    {
        app->remap_prompt = false;
        return;
    }
    if (app->remap_prompt)
    {
        uint32_t controller = app->remap_prompt_controller;
        bool held = input_controller_action_held(input, controller, InputAction::pad_start);
        if (!held)
        {
            app->remap_prompt_start_held_ms = 0;
        }
        else if (app->remap_prompt_start_held_ms == 0)
        {
            app->remap_prompt_start_held_ms = now_ms;
        }
        if (held && now_ms - app->remap_prompt_start_held_ms >= REMAP_HOLD_MS)
        {
            app->remap_prompt = false;
            quick_menu_open_remap(&app->quick_menu, app->session.state_slot, app->remap_prompt_port, controller);
        }
        else if (now_ms - app->remap_prompt_started_ms >= REMAP_PROMPT_MS)
        {
            app->remap_prompt = false;
            // (The game's remap if it has one, else the core's.)
            std::string id = input_controller_profile_id(input, controller);
            uint32_t device = input->port_devices[app->remap_prompt_port];
            bool game_has = input_core_profile(input, input->core_name.c_str(), input->game_name.c_str(), device, id) != nullptr;
            InputCoreProfile* profile = input_core_profile_for(input, input->core_name.c_str(),
                                                               game_has ? input->game_name.c_str() : "", device, id);
            profile->decided = true;   // the RetroPad, remembered
            if (!app->input_config_path.empty())
            {
                input_config_save(input, app->input_config_path.c_str());
            }
        }
        return;
    }
    if (ui_is_up(app))
    {
        return;   // asked when the game is playing
    }
    for (uint32_t port = 0; port < INPUT_PORT_COUNT; port++)
    {
        for (size_t m = 0; m < input->ports[port].size(); m++)
        {
            uint32_t controller = input->ports[port][m];
            const InputCoreProfile* profile = input_active_core_profile(input, controller, port);
            if (profile == nullptr || !profile->decided)
            {
                app->remap_prompt = true;
                app->remap_prompt_controller = controller;
                app->remap_prompt_port = port;
                app->remap_prompt_started_ms = now_ms;
                app->remap_prompt_start_held_ms = 0;
                return;
            }
        }
    }
}

// The prompt as a line at the top of the window.
static void draw_remap_prompt(App* app)
{
    if (!app->remap_prompt)
    {
        return;
    }
    const LibretroHost* host = app->session.host;
    uint32_t port = app->remap_prompt_port;
    const char* device = libretro_input_controller_name(&host->input_info, port, host->port_devices[port]);
    uint64_t now = SDL_GetTicks();
    uint64_t elapsed = now - app->remap_prompt_started_ms;
    int left = static_cast<int>((REMAP_PROMPT_MS - (elapsed < REMAP_PROMPT_MS ? elapsed : REMAP_PROMPT_MS) + 999) / 1000);
    char text[400] = {};
    std::snprintf(text, sizeof(text), piccolo_text(&app->strings, Str::remap_prompt), device != nullptr ? device : "RETROPAD",
                  static_cast<int>(port + 1), input_controller_label(&app->input, app->remap_prompt_controller).c_str(), left);
    retro_ui_push_theme(app->pixel_font, app->ui_scale);
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, 16.0f * static_cast<float>(app->ui_scale)), ImGuiCond_Always,
                            ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##remap_prompt", nullptr, flags))
    {
        retro_ui_begin_window(app->ui_scale);
        ImGui::TextUnformatted(text);
        retro_ui_end_window();
    }
    ImGui::End();
    retro_ui_pop_theme();
}

// MAP CONTROLLER offered: once per session for a controller SDL doesn't
// know (it can't be used otherwise), and the first time a known one joins
// (unless mapped or declined before). Its results go to the status line
// and input.cfg.
static void offer_mapping(App* app, const InputFrame* frame)
{
    MapWindow* map = &app->map_window;
    uint64_t now = SDL_GetTicks();
    if (!map_window_open_now(map))
    {
        for (size_t u = 0; u < app->input_sdl.unmapped.size(); u++)
        {
            uint32_t joystick = app->input_sdl.unmapped[u];
            bool offered = false;
            for (size_t o = 0; o < app->map_offered.size(); o++)
            {
                offered = offered || app->map_offered[o] == joystick;
            }
            if (!offered)
            {
                app->map_offered.push_back(joystick);
                map_window_offer(map, joystick, true, &app->layouts, &app->input, now);
                break;
            }
        }
    }
    if (!map_window_open_now(map) && frame->joined_controller >= 0)
    {
        uint32_t device_id = app->input.controllers[static_cast<size_t>(frame->joined_controller)].device_id;
        const InputDevice* device = input_devices_find_const(&app->input.devices, device_id);
        if (device != nullptr && device->kind == InputDeviceKind::gamepad)
        {
            const InputProfile* profile = input_profile(&app->input, device->guid);
            uint32_t joystick = input_sdl_joystick_of(&app->input_sdl, device_id);
            if ((profile == nullptr || profile->appearance.empty()) && joystick != 0)
            {
                map_window_offer(map, joystick, false, &app->layouts, &app->input, now);
            }
        }
    }
    if (app->input_window.map_request != 0 && !map_window_open_now(map))
    {
        map_window_open(map, app->input_window.map_request, &app->layouts, &app->input);
    }
    app->input_window.map_request = 0;
    if (!map->result.empty())
    {
        set_status(app, map->result.c_str());
        map->result.clear();
    }
    if (map->profiles_dirty && !app->input_config_path.empty())
    {
        input_config_save(&app->input, app->input_config_path.c_str());
        map->profiles_dirty = false;
    }
}

// The ports' cursors for the core: from the part of the frame on screen to
// the whole frame (a crop shows less of it). And the mouse grabbed while a
// port's core device is a mouse with a mouse on it (no UI up, not let go
// with MOUSE GRAB).
static void update_cursors(App* app, const InputFrame* frame, bool ui_up)
{
    for (uint32_t p = 0; p < INPUT_PORT_COUNT && p < LIBRETRO_MAX_PORTS; p++)
    {
        LibretroCursorState* cursor = &app->cursor_input.cursor[p];
        float x = (static_cast<float>(frame->cursor_x[p]) / 32767.0f + 1.0f) * 0.5f;
        float y = (static_cast<float>(frame->cursor_y[p]) / 32767.0f + 1.0f) * 0.5f;
        if (app->view_frame[0] > 0 && app->view_frame[1] > 0 && app->view_source[2] > 0 && app->view_source[3] > 0)
        {
            x = (static_cast<float>(app->view_source[0]) + x * static_cast<float>(app->view_source[2])) /
                static_cast<float>(app->view_frame[0]);
            y = (static_cast<float>(app->view_source[1]) + y * static_cast<float>(app->view_source[3])) /
                static_cast<float>(app->view_frame[1]);
        }
        cursor->x = static_cast<int16_t>(x * 65534.0f - 32767.0f);
        cursor->y = static_cast<int16_t>(y * 65534.0f - 32767.0f);
        cursor->off_screen = frame->cursor_off[p] || app->input.view_width <= 0.0f;
        cursor->mouse_dx = frame->mouse_dx[p];
        cursor->mouse_dy = frame->mouse_dy[p];
        cursor->buttons = frame->mouse_buttons[p];   // (the same bits: left, right, middle, wheel, reload, the lightgun's)
    }
    if (frame->pressed[static_cast<uint32_t>(InputAction::hotkey_grab_mouse)])
    {
        app->mouse_released = !app->mouse_released;
    }
    bool mouse_port = false;
    for (uint32_t p = 0; game_running(app) && p < INPUT_PORT_COUNT; p++)
    {
        if ((app->session.host->port_devices[p] & RETRO_DEVICE_MASK) != RETRO_DEVICE_MOUSE)
        {
            continue;
        }
        for (size_t m = 0; m < app->input.ports[p].size(); m++)
        {
            uint32_t controller = app->input.ports[p][m];
            const InputDevice* device = input_devices_find_const(&app->input.devices, app->input.controllers[controller].device_id);
            mouse_port = mouse_port || (device != nullptr && device->kind == InputDeviceKind::mouse);
        }
    }
    bool grab = mouse_port && !ui_up && !app->mouse_released && app->window_focused;
    if (grab != app->mouse_grabbed)
    {
        SDL_SetWindowRelativeMouseMode(app->shell.window, grab);
        app->mouse_grabbed = grab;
    }
}

// Once per loop, after the events: what the controllers say (ports,
// hotkeys), handed to the core session before it runs frames.
static void app_update_input(App* app)
{
    bool ui_up = ui_is_up(app);
    app->input.settings.ui_active = ui_up;   // the INTERFACE layer
    piccolo_settings_input(&app->settings, &app->input.settings);   // SETTINGS > INPUT, per core / game too
    // The core and the device on each port: a controller's bindings for them.
    app->input.core_name = game_running(app) ? app->core_name : "";
    app->input.game_name = game_running(app) ? app->game_name : "";   // a game's own remaps
    for (unsigned port = 0; port < LIBRETRO_MAX_PORTS; port++)
    {
        app->input.port_devices[port] = app->session.host->port_devices[port];
    }
    InputFrame frame = {};
    input_update(&app->input, SDL_GetTicks(), &frame);
    update_remap_prompt(app, SDL_GetTicks());
    offer_mapping(app, &frame);
    update_cursors(app, &frame, ui_up);
    if (frame.profiles_changed && !app->input_config_path.empty())
    {
        input_config_save(&app->input, app->input_config_path.c_str());   // a turbo's speed, by taps (in the game)
    }
    char message[300] = {};
    if (frame.joined_port != INPUT_NO_PORT && frame.joined_controller >= 0)
    {
        std::snprintf(message, sizeof(message), piccolo_text(&app->strings, Str::input_joined),
                      input_controller_label(&app->input, static_cast<uint32_t>(frame.joined_controller)).c_str(), frame.joined_port + 1);
        set_status(app, message);
        SDL_Log("%s", message);
    }
    if (frame.left_port != INPUT_NO_PORT && frame.left_controller >= 0)
    {
        std::snprintf(message, sizeof(message), piccolo_text(&app->strings, Str::input_left),
                      input_controller_label(&app->input, static_cast<uint32_t>(frame.left_controller)).c_str(), frame.left_port + 1);
        set_status(app, message);
        SDL_Log("%s", message);
        if (game_running(app) && !app->quick_menu.open)
        {
            quick_menu_open_controls(&app->quick_menu, app->session.state_slot);   // rearrange the ports
        }
    }
    app_handle_hotkeys(app, &frame);
    drive_ui_with_gamepads(app, &frame, ui_up);
    if (frame.keyboard_pressed[static_cast<uint32_t>(InputAction::ui_back)])
    {
        close_topmost_or_toggle_menu(app);   // Escape in the UI: back, at once
    }
    // PAGE LEFT / RIGHT change tabs (not while typing in a field).
    int page = 0;
    if (ui_up && !ImGui::GetIO().WantTextInput)
    {
        page = frame.pressed[static_cast<uint32_t>(InputAction::ui_page_right)] ? 1
             : frame.pressed[static_cast<uint32_t>(InputAction::ui_page_left)]  ? -1 : 0;
    }
    retro_ui_set_page_request(page);
    core_session_set_pads(&app->session, frame.pad_buttons, frame.analog);
    bool net_fast_forward = false;
    for (uint32_t i = 0; i < NET_MAX_PEERS; i++)
    {
        net_fast_forward = net_fast_forward || app->net_fast_forward[i];
    }
    app->session.fast_forward_held =
        game_running(app) && (frame.held[static_cast<uint32_t>(InputAction::hotkey_fast_forward)] || net_fast_forward);
    if (app->session.host != nullptr)
    {
        input_sdl_rumble(&app->input_sdl, &app->input, app->session.host->rumble, LIBRETRO_MAX_PORTS);
    }
}

static void app_handle_events(App* app)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (core_session_handle_event(&app->session, &app->shell, &event, game_has_keyboard(app)))
        {
            continue;
        }
        // While INPUT waits for a key (SET), keys are the binding's, not ImGui's.
        bool key_event = event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_TEXT_INPUT;
        if (!(key_event && (app->input_window.capturing || app->quick_menu.capturing)))
        {
            app_shell_process_event(&app->shell, &event);
        }
        input_sdl_handle_event(&app->input_sdl, &app->input, &event);
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE && app->input.suspended)
        {
            map_window_close(&app->map_window, &app->input);   // the router is off while mapping: Escape still gives up
        }

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
                case SDLK_F11:
                    toggle_fullscreen(app);
                    break;
                case SDLK_RETURN:
                    if (event.key.mod & SDL_KMOD_ALT)
                    {
                        toggle_fullscreen(app);
                    }
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
    app->input.view_width = 0.0f;   // (until it is drawn)
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
        // Where the cursors live (window pixels), and the part of the frame it shows.
        app->input.view_x = static_cast<float>(viewport.x);
        app->input.view_y = static_cast<float>(viewport.y);
        app->input.view_width = static_cast<float>(viewport.width);
        app->input.view_height = static_cast<float>(viewport.height);
        app->view_source[0] = viewport.source_x;
        app->view_source[1] = viewport.source_y;
        app->view_source[2] = viewport.source_width;
        app->view_source[3] = viewport.source_height;
        app->view_frame[0] = input.frame_width;
        app->view_frame[1] = input.frame_height;
        // The background draw list is behind every window, menu bar included.
        picture_draw(&app->picture, ImGui::GetBackgroundDrawList(), &session->texture, session->texture_id, 0.0f, 0.0f);
    }
}

// A folder field or GAME FOLDERS' "+" in SETTINGS: CHOOSE FOLDER, starting
// at the folder the setting has (or the default one, or the first game
// folder).
static void choose_folder_start(App* app)
{
    const char* key = app->settings_window.choose_folder_key;
    app->settings_window.choose_folder_key = nullptr;
    if (key == nullptr)
    {
        return;
    }
    std::vector<std::string> game_folders;
    piccolo_settings_game_folders(&app->settings, &game_folders);
    // Not set: where the default is (what the empty field shows).
    const char* start = settings_get_text(&app->settings, key);
    if (start[0] == '\0' && std::strcmp(key, PICCOLO_PATH_CORES) == 0)
    {
        start = app->paths.cores.c_str();
    }
    else if (start[0] == '\0' && std::strcmp(key, PICCOLO_PATH_SYSTEM) == 0)
    {
        start = app->paths.system.c_str();
    }
    else if (start[0] == '\0' && std::strcmp(key, PICCOLO_PATH_SAVES) == 0)
    {
        start = app->paths.saves.c_str();
    }
    else if (start[0] == '\0' && std::strcmp(key, PICCOLO_PATH_STATES) == 0)
    {
        start = app->paths.states.c_str();
    }
    app->choose_folder_key = key;
    file_picker_open_folder(&app->picker, start, &game_folders);
}

static bool is_game_folder_key(const char* key, uint32_t* slot)
{
    for (uint32_t i = 0; i < PICCOLO_GAME_FOLDER_MAX; i++)
    {
        if (std::strcmp(key, PICCOLO_PATH_GAMES[i]) == 0)
        {
            *slot = i;
            return true;
        }
    }
    return false;
}

static void choose_folder_done(App* app, const FilePickerChoice* picked)
{
    const char* key = app->choose_folder_key;
    app->choose_folder_key = nullptr;
    if (key == nullptr)
    {
        return;
    }
    uint32_t wanted = 0;
    if (!is_game_folder_key(key, &wanted))
    {
        settings_set(&app->settings, SettingLayer::global, key, picked->content_path);
    }
    else
    {
        // Into that row, or the first empty one: the slots have no gaps, and
        // a row may have been removed while the picker was open.
        uint32_t slot = 0;
        while (slot < wanted && settings_get_text(&app->settings, PICCOLO_PATH_GAMES[slot])[0] != '\0')
        {
            slot++;
        }
        for (uint32_t i = 0; i < PICCOLO_GAME_FOLDER_MAX; i++)
        {
            if (i != slot &&
                std::strcmp(settings_get_text(&app->settings, PICCOLO_PATH_GAMES[i]), picked->content_path) == 0)
            {
                return;   // already in the list
            }
        }
        settings_set(&app->settings, SettingLayer::global, PICCOLO_PATH_GAMES[slot], picked->content_path);
    }
    if (!app->settings_window.open)
    {
        apply_folders(app);
        save_settings(app);   // SETTINGS closed (and saved) while the folder was being chosen
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
    context.themes = &app->themes;
    NetWindowContext network;
    network.settings = &app->settings;
    network.strings = &app->strings;
    network.access = app->net_running ? &app->net : nullptr;
    network.listen_failed = app->net_listen_failed;
    network.this_device = app->net_fingerprint;
    context.network = &network;
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
    settings_window_draw(&app->settings_window, &context, app->pixel_font, app->ui_scale);
    choose_folder_start(app);
    if (app->settings_window.search_frame_delay)
    {
        app->settings_window.search_frame_delay = false;
        search_frame_delay_again(app);
    }
    // SAVE THEME: into themes/ in piccolo's folder, then listed in THEME.
    if (!app->settings_window.save_theme_request.empty())
    {
        std::string name = app->settings_window.save_theme_request;
        app->settings_window.save_theme_request.clear();
        if (app->paths.root.empty() ||
            !piccolo_themes_save(&app->settings, name.c_str(), (app->paths.root + "/themes").c_str(), &app->themes))
        {
            set_status(app, piccolo_text(&app->strings, Str::ui_theme_save_failed));
        }
        else
        {
            app->settings_window.theme_choice = name;   // CUSTOM THEME shows the saved theme
            app->settings_window.theme_custom = true;
        }
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

// The quick menu or the options manager closed: what was changed there in
// the core options is written now. (While they are open it waits, so an
// option that crashes the core at once is not kept.)
static void save_after_options_windows(App* app)
{
    bool open = app->quick_menu.open || app->core_options_manager;
    if (app->options_window_was_open && !open)
    {
        save_core_options(app);
        save_settings(app);   // a port's DEVICE (CONTROLS)
        if (app->quick_menu.input_dirty && !app->input_config_path.empty())
        {
            input_config_save(&app->input, app->input_config_path.c_str());   // a remap (CONTROLS)
            app->quick_menu.input_dirty = false;
        }
    }
    app->options_window_was_open = open;
}

static void app_draw_debug_ui(App* app)
{
    const ImGuiIO& io = ImGui::GetIO();
    const VkPhysicalDeviceProperties* gpu = &app->shell.vulkan.physical_device_properties;
    const VulkanSwapchain* swapchain = &app->shell.swapchain;

    ImGui::SetNextWindowPos(ImVec2(40.0f, 80.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("piccolo", &app->show_debug_window);

    ImGui::Text("piccolo %s, host-libretro %s", PICCOLO_VERSION, HOST_LIBRETRO_VERSION);
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

// game_state (what runs now) to one authorized peer, or to all of them
// (NET_NO_PEER).
static void net_send_game_state(App* app, uint32_t peer)
{
    size_t size = net_games_write_state(&app->net_games, &app->catalog, app->net_sent_core.c_str(),
                                        app->net_sent_content.c_str());
    for (uint32_t i = 0; i < NET_MAX_PEERS; i++)
    {
        if ((peer == NET_NO_PEER || peer == i) && app->net.peers[i].state == NetAccessPeerState::authorized)
        {
            net_access_send(&app->net, i, NetChannel::control, app->net_games.reply.data(), size);
        }
    }
}

// Tells every device when what runs changed, however it changed (LOAD GAME,
// the network, CLOSE GAME, a core that quit). Compared every loop, copied
// only on a change.
static void net_follow_game_state(App* app)
{
    bool running = game_running(app);
    const char* core = running ? app->marker_core_path.c_str() : "";
    const char* content = running ? app->marker_content_path.c_str() : "";
    if (app->net_sent_core == core && app->net_sent_content == content)
    {
        return;
    }
    app->net_sent_core = core;
    app->net_sent_content = content;
    net_send_game_state(app, NET_NO_PEER);
}

static NetRemoteGame net_remote_game(const App* app)
{
    NetRemoteGame game;
    game.running = game_running(app);
    game.slot = app->session.state_slot;
    game.paused = app->net_paused;
    return game;
}

static NetAccessLevel net_peer_access(const App* app, uint32_t peer)
{
    int32_t device = app->net.peers[peer].device;
    if (device < 0 || static_cast<size_t>(device) >= app->net.trust.devices.size())
    {
        return NetAccessLevel::gamepad;
    }
    return app->net.trust.devices[static_cast<size_t>(device)].access;
}

// The actions one authorized peer may trigger (`id` 0: sent unasked), or
// to all of them (NET_NO_PEER), each with its own level.
static void net_send_actions(App* app, uint32_t peer, uint32_t id)
{
    if (!app->net_remote_ready)
    {
        return;
    }
    NetRemoteGame game = net_remote_game(app);
    for (uint32_t i = 0; i < NET_MAX_PEERS; i++)
    {
        if ((peer == NET_NO_PEER || peer == i) && app->net.peers[i].state == NetAccessPeerState::authorized)
        {
            size_t size = net_remote_write_actions(&app->net_remote, id, app->net_peer_language[i],
                                                   app->paths.root.c_str(), net_peer_access(app, i), &game);
            net_access_send(&app->net, i, NetChannel::control, app->net_remote.reply.data(), size);
        }
    }
}

// Every loop: settings that changed (from anywhere) go to the FULL devices,
// and the action lists again when what they show changed.
static void net_follow_remote(App* app)
{
    if (!app->net_remote_ready)
    {
        return;
    }
    size_t size = net_remote_next_change(&app->net_remote, &app->settings);
    while (size > 0)
    {
        for (uint32_t i = 0; i < NET_MAX_PEERS; i++)
        {
            if (app->net.peers[i].state == NetAccessPeerState::authorized &&
                net_peer_access(app, i) == NetAccessLevel::full)
            {
                net_access_send(&app->net, i, NetChannel::control, app->net_remote.reply.data(), size);
            }
        }
        size = net_remote_next_change(&app->net_remote, &app->settings);
    }
    // A PAUSE from a device ends with another game (or none).
    if (app->net_paused && game_running(app) != app->net_actions_sent.running)
    {
        app->net_paused = false;
    }
    NetRemoteGame game = net_remote_game(app);
    if (game.running != app->net_actions_sent.running || game.slot != app->net_actions_sent.slot ||
        game.paused != app->net_actions_sent.paused)
    {
        app->net_actions_sent = game;
        net_send_actions(app, NET_NO_PEER, 0);
    }
}

// An action from a device, with the same effect as its hotkey or quick
// menu item.
static void net_do_action(App* app, uint32_t peer, const std::string* key, bool pressed, const char* device)
{
    NetRemoteGame game = net_remote_game(app);
    if (!net_remote_action_allowed(key->c_str(), net_peer_access(app, peer), &game))
    {
        return;
    }
    if (*key == "fast_forward")
    {
        app->net_fast_forward[peer] = pressed;   // held until released (or the device leaves)
        return;
    }
    if (!pressed)
    {
        return;   // the others act on the press
    }
    if (*key == "save_state")
    {
        handle_menu_action(app, MenuAction::save_state);
    }
    else if (*key == "load_state")
    {
        handle_menu_action(app, MenuAction::load_state);
    }
    else if (*key == "slot_next" || *key == "slot_previous")
    {
        uint32_t slot = app->session.state_slot;
        if (*key == "slot_next")
        {
            slot = slot >= LIBRETRO_STATE_SLOT_LAST ? LIBRETRO_STATE_SLOT_FIRST : slot + 1;
        }
        else
        {
            slot = slot <= LIBRETRO_STATE_SLOT_FIRST ? LIBRETRO_STATE_SLOT_LAST : slot - 1;
        }
        app->session.state_slot = slot;
        app->quick_menu.slot = slot;
        char status[64];
        std::snprintf(status, sizeof(status), "%s %u", piccolo_text(&app->strings, Str::quick_menu_slot), slot);
        set_status(app, status);
    }
    else if (*key == "pause")
    {
        app->net_paused = !app->net_paused;
        app->net_paused_by = peer;
        char status[256];
        if (app->net_paused)
        {
            std::snprintf(status, sizeof(status), "%s %s", piccolo_text(&app->strings, Str::status_paused_remotely),
                          device);
        }
        else
        {
            std::snprintf(status, sizeof(status), "%s", piccolo_text(&app->strings, Str::status_resumed));
        }
        set_status(app, status);
    }
    else if (*key == "reset")
    {
        handle_menu_action(app, MenuAction::reset);
    }
    else if (*key == "close_game")
    {
        app->pending_close_game = true;   // at the top of the next loop, like the quick menu's
    }
}

// Settings and actions messages from a device (net_remote.h).
static void net_remote_message(App* app, const NetAccessEvent* event, const char* device)
{
    if (!app->net_remote_ready)
    {
        return;
    }
    size_t size = net_remote_answer_settings(&app->net_remote, &app->settings, app->paths.root.c_str(),
                                             &app->audio_devices, event->data, event->size);
    if (size > 0)
    {
        net_access_send(&app->net, event->peer, NetChannel::control, app->net_remote.reply.data(), size);
        uint32_t request_id = 0;
        uint32_t start = 0;
        std::string language;
        if (event->peer < NET_MAX_PEERS &&
            net_settings_read_request(event->data, event->size, &request_id, &start, &language))
        {
            std::snprintf(app->net_peer_language[event->peer], sizeof(app->net_peer_language[event->peer]), "%s",
                          language.c_str());
        }
        bool changed = event->data[0] == static_cast<uint8_t>(NetSettingsMessage::setting_set);
        if (changed && !app->settings_window.open)
        {
            save_settings(app);   // nobody will close SETTINGS to save it
        }
        return;
    }
    uint32_t id = 0;
    std::string language;
    if (net_settings_read_actions_request(event->data, event->size, &id, &language))
    {
        if (event->peer < NET_MAX_PEERS)
        {
            std::snprintf(app->net_peer_language[event->peer], sizeof(app->net_peer_language[event->peer]), "%s",
                          language.c_str());
        }
        NetRemoteGame game = net_remote_game(app);
        size = net_remote_write_actions(&app->net_remote, id, language.c_str(), app->paths.root.c_str(),
                                        net_peer_access(app, event->peer), &game);
        net_access_send(&app->net, event->peer, NetChannel::control, app->net_remote.reply.data(), size);
        return;
    }
    std::string key;
    bool pressed = false;
    if (net_settings_read_action(event->data, event->size, &key, &pressed))
    {
        net_do_action(app, event->peer, &key, pressed, device);
    }
}

// The load_result for the load_request waiting in pending_start.
static void net_answer_load(App* app, NetContentStatus status)
{
    app->net_load_waiting = false;
    if (!app->net_running)
    {
        return;
    }
    uint8_t reply[16];
    size_t size = net_content_write_load_result(reply, sizeof(reply), app->net_load_id, status);
    net_access_send(&app->net, app->net_load_peer, NetChannel::control, reply, size);
}

// A control message from a paired device (net_games.h). Folders and cores
// are read when asked, never every frame.
static void net_games_message(App* app, const NetAccessEvent* event, const char* device)
{
    uint8_t type = event->size > 0 ? event->data[0] : 0;
    if (type < static_cast<uint8_t>(NetContentMessage::cores_request) ||
        type > static_cast<uint8_t>(NetContentMessage::load_result))
    {
        return;   // not ours (yet)
    }
    std::vector<std::string> game_folders;
    piccolo_settings_game_folders(&app->settings, &game_folders);
    net_games_set_folders(&app->net_games, &game_folders);

    size_t size = net_games_answer(&app->net_games, &app->catalog, event->data, event->size);
    if (size > 0)
    {
        net_access_send(&app->net, event->peer, NetChannel::control, app->net_games.reply.data(), size);
        return;
    }
    uint32_t id = 0;
    NetContentStatus status = NetContentStatus::failed;
    FilePickerChoice choice;
    if (!net_games_load(&app->net_games, &app->catalog, event->data, event->size, &id, &status, &choice))
    {
        return;   // a result type, or a damaged request: nothing to answer
    }
    if (status != NetContentStatus::ok || app->has_pending_start || app->net_load_waiting)
    {
        if (status == NetContentStatus::ok)
        {
            status = NetContentStatus::failed;   // another start is already on its way
        }
        uint8_t reply[16];
        size_t reply_size = net_content_write_load_result(reply, sizeof(reply), id, status);
        net_access_send(&app->net, event->peer, NetChannel::control, reply, reply_size);
        return;
    }
    SDL_Log("Network: %s starts %s %s", device, choice.core_path, choice.content_path);
    app->pending_start = choice;   // started at the top of the next loop, like LOAD GAME's
    app->has_pending_start = true;
    app->picker.open = false;      // what was being picked here is overtaken
    app->net_load_waiting = true;
    app->net_load_peer = event->peer;
    app->net_load_id = id;
}

// The listener, when SETTINGS has it on. Started at launch and restarted
// when ALLOW CONNECTIONS or PORT change (net_update).
static void net_start(App* app)
{
    // Any failure below is remembered with the port it was tried on, so it
    // isn't retried every frame (net_update).
    app->net_port = static_cast<uint16_t>(settings_get_int(&app->settings, PICCOLO_NET_PORT));
    app->net_listen_failed = true;
    if (app->paths.settings.empty())
    {
        return;   // nowhere to keep the device key: networking stays off
    }
    if (!app->net_ready)
    {
        app->net_ready = net_init();
        if (!app->net_ready)
        {
            return;
        }
    }
    if (!app->net_has_identity)
    {
        std::string identity_path = app->paths.settings + "/net_identity.cfg";
        app->net_has_identity = net_identity_load_or_create(identity_path.c_str(), &app->net_identity);
        if (!app->net_has_identity)
        {
            return;
        }
        net_key_fingerprint(app->net_identity.public_key, app->net_fingerprint, sizeof(app->net_fingerprint));
        SDL_Log("Network: this device is %s", app->net_fingerprint);
    }
    app->net = NetAccess{};
    net_games_init(&app->net_games);
    if (!app->net_remote_ready)
    {
        app->net_remote_ready = net_remote_init(&app->net_remote, &app->settings);
    }
    std::string devices_path = app->paths.settings + "/net_devices.cfg";
    if (!net_access_create(&app->net, app->net_port, settings_get_text(&app->settings, PICCOLO_NET_NAME),
                           &app->net_identity, devices_path.c_str()))
    {
        net_access_destroy(&app->net);
        return;
    }
    app->net_listen_failed = false;
    app->net_running = true;
}

static void net_stop(App* app)
{
    if (app->net_running)
    {
        for (uint32_t peer = 0; peer < NET_MAX_PEERS; peer++)
        {
            input_net_peer_gone(&app->input_net, &app->input, peer);   // their pads are unplugged
        }
        net_access_destroy(&app->net);
        app->net_running = false;
    }
}

static void net_shutdown_all(App* app)
{
    net_stop(app);
    net_keypair_wipe(&app->net_identity);
    if (app->net_ready)
    {
        net_shutdown();
        app->net_ready = false;
    }
}

// Once per loop, before the input is read: follow the settings, then the
// network's events. Paired devices are let in and their pads join the input
// router (input_net); everyone else needs the pairing code.
static void net_update(App* app)
{
    bool enabled = settings_get_bool(&app->settings, PICCOLO_NET_ENABLED);
    uint16_t port = static_cast<uint16_t>(settings_get_int(&app->settings, PICCOLO_NET_PORT));
    if (app->net_running && (!enabled || port != app->net_port))
    {
        net_stop(app);
        app->net_listen_failed = false;
    }
    // After a failure (port in use) it's tried again only with another port.
    if (enabled && !app->net_running && (!app->net_listen_failed || port != app->net_port))
    {
        net_start(app);
    }
    if (!enabled)
    {
        app->net_listen_failed = false;
    }
    if (!app->net_running)
    {
        return;
    }
    // A new NAME is used at once (discovery and pairing show it).
    const char* name = settings_get_text(&app->settings, PICCOLO_NET_NAME);
    if (app->net.name != name)
    {
        app->net.name = name;
        app->net.session.host.name = name;
    }

    NetAccessEvent event;
    while (net_access_poll(&app->net, &event))
    {
        char address[64];
        net_address_to_text(event.address, address, sizeof(address));
        const char* device = event.device >= 0 && static_cast<size_t>(event.device) < app->net.trust.devices.size()
                                 ? app->net.trust.devices[static_cast<size_t>(event.device)].name.c_str()
                                 : "?";
        switch (event.type)
        {
        case NetAccessEventType::authorized:
            SDL_Log("Network: \"%s\" connected from %s", device, address);
            net_send_game_state(app, event.peer);   // what runs, from the start
            net_send_actions(app, event.peer, 0);
            break;
        case NetAccessEventType::paired:
        {
            char status[256];
            std::snprintf(status, sizeof(status), "%s %s", piccolo_text(&app->strings, Str::status_net_paired), device);
            set_status(app, status);
            net_send_game_state(app, event.peer);
            net_send_actions(app, event.peer, 0);
            break;
        }
        case NetAccessEventType::closed:
            if (event.peer < NET_MAX_PEERS)
            {
                app->net_fast_forward[event.peer] = false;   // a dropped device doesn't keep it held
                app->net_peer_language[event.peer][0] = '\0';
                if (app->net_paused && app->net_paused_by == event.peer)
                {
                    app->net_paused = false;   // nobody here could resume it
                    set_status(app, piccolo_text(&app->strings, Str::status_resumed));
                }
            }
            if (app->net_load_waiting && app->net_load_peer == event.peer)
            {
                app->net_load_waiting = false;   // the game still starts; nobody to tell
            }
            SDL_Log("Network: \"%s\" disconnected", device);
            input_net_peer_gone(&app->input_net, &app->input, event.peer);
            break;
        case NetAccessEventType::pairing_failed:
            set_status(app, piccolo_text(&app->strings, Str::status_net_pairing_failed));
            break;
        case NetAccessEventType::pairing_closed:
            set_status(app, piccolo_text(&app->strings, Str::status_net_pairing_closed));
            break;
        case NetAccessEventType::message:
            if (event.channel == NetChannel::input && event.device >= 0)
            {
                const NetTrustedDevice* paired = &app->net.trust.devices[static_cast<size_t>(event.device)];
                input_net_receive(&app->input_net, &app->input, event.peer, paired->key, paired->name.c_str(),
                                  event.data, event.size);
            }
            else if (event.channel == NetChannel::control)
            {
                net_games_message(app, &event, device);   // net_access let through only what its level covers
                net_remote_message(app, &event, device);
            }
            break;
        case NetAccessEventType::none:
            break;
        }
    }
    net_follow_game_state(app);
    net_follow_remote(app);
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    SDL_Log("piccolo %s, host-libretro %s", PICCOLO_VERSION, HOST_LIBRETRO_VERSION);

    // The session, catalog and file picker are large (paths, file lists): off the stack.
    App* app = new App{};

    AppShellConfig config = {};
    config.name       = "piccolo";
    config.identifier = "org.multitap.piccolo";
    config.icons      = app_icon_images;
    config.icon_count = app_icon_image_count;

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
    debug_ui_set_draw_data_hook(add_text_shadows, app);
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
        piccolo_settings_complete_ui_theme(&app->settings);
        apply_folders(app);
        load_last_used(app);
        app->recent_path = app->paths.settings + "/recent.cfg";
        recent_games_load(&app->recent, app->recent_path.c_str());
        app->audio_learned_path = app->paths.settings + "/audio_latency.cfg";
        settings_file_load(&app->audio_learned, app->audio_learned_path.c_str());
        app->running_marker_path = app->paths.settings + "/running.cfg";
        check_running_marker(app);
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
    // Controllers: MAP CONTROLLER's mappings are in their profiles (input.cfg);
    // settings/gamecontrollerdb.txt, if the user drops one in (SDL's format),
    // adds to the ones SDL knows.
    std::string database_path = app->paths.settings.empty() ? "" : app->paths.settings + "/gamecontrollerdb.txt";
    // Controller layouts: the ones next to the program, then piccolo's folder's
    // (the same id there wins), and GENERIC built in if neither has it.
    const char* program_folder = SDL_GetBasePath();
    std::string bundled_layouts = std::string(program_folder != nullptr ? program_folder : "") + "layouts";
    host_fs_forward_slashes(bundled_layouts.data());
    input_layouts_load_directory(&app->layouts, bundled_layouts.c_str());
    if (!app->paths.root.empty())
    {
        input_layouts_load_directory(&app->layouts, (app->paths.root + "/layouts").c_str());
    }
    input_layouts_ensure_generic(&app->layouts);
    // Interface themes from files: the ones next to the program, then piccolo's
    // folder's (the same name there wins). The built-in ones are in retro_ui.cpp.
    std::string bundled_themes = std::string(program_folder != nullptr ? program_folder : "") + "themes";
    host_fs_forward_slashes(bundled_themes.data());
    piccolo_themes_load_directory(&app->themes, bundled_themes.c_str());
    if (!app->paths.root.empty())
    {
        piccolo_themes_load_directory(&app->themes, (app->paths.root + "/themes").c_str());
    }
    // settings/input.cfg first: controllers are made from its profiles as their devices appear.
    app->input_config_path = app->paths.settings.empty() ? "" : app->paths.settings + "/input.cfg";
    if (!app->input_config_path.empty())
    {
        input_config_load(&app->input, app->input_config_path.c_str());
    }
    input_sdl_init(&app->input_sdl, &app->input, database_path.empty() ? nullptr : database_path.c_str());
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
        net_update(app);
        if (app->pending_close_game)
        {
            app->pending_close_game = false;
            save_core_options(app);
            core_session_stop(&app->session, &app->shell);   // writes save RAM
            remove_running_marker(app);   // a proper end
            update_settings_context(app);
            app->menu_visible = true;
            set_status(app, piccolo_text(&app->strings, Str::status_game_closed));
        }
        if (app->has_pending_start)
        {
            app->has_pending_start = false;
            bool started = start_game(app, &app->pending_start);   // outside any frame: nothing drawn with the old texture yet
            // SAFE START asking: answered once it's decided (below, or the
            // start that follows its answer).
            if (app->net_load_waiting && !app->safe_start_open)
            {
                net_answer_load(app, started ? NetContentStatus::ok : NetContentStatus::failed);
            }
        }
        update_pause(app);
        app->session.timing.mode = piccolo_settings_timing(&app->settings);
        app->session.timing.catch_up = settings_get_bool(&app->settings, PICCOLO_VIDEO_CATCH_UP);
        update_window(app);
        update_language(app);
        RetroUiPalette palette = {};
        piccolo_settings_palette(&app->settings, &palette);   // the Interface settings, live
        retro_ui_set_palette(&palette);
        // BACKGROUND behind the menu; while a game runs, black around its picture.
        uint32_t back = game_running(app) ? 0xFF000000u : palette.background;
        ClearColor background = {static_cast<float>((back >> 16) & 0xFFu) / 255.0f,
                                 static_cast<float>((back >> 8) & 0xFFu) / 255.0f,
                                 static_cast<float>(back & 0xFFu) / 255.0f, 1.0f};
        renderer_set_frames_in_flight(&app->shell.renderer, &app->shell.vulkan,
                                      piccolo_settings_frames_in_flight(&app->settings));
        app->shell.renderer.wait_for_present = settings_get_bool(&app->settings, PICCOLO_LATENCY_WAIT_FOR_PRESENT);
        piccolo_settings_audio(&app->settings, &app->session.audio_config);
        apply_learned_latency(app);
        // Held for the whole game, paused or not: a request is a D-Bus call
        // that may block, so not on every pause and resume.
        game_mode_set(&app->game_mode, game_running(app) && settings_get_bool(&app->settings, PICCOLO_GAME_MODE));
        core_session_update(&app->session, &app->shell, game_has_keyboard(app));
        if (app->option_cache.dirty)
        {
            save_core_options(app);   // a core declared new options: on disk before it can crash
        }
        if (app->marker_written && !app->marker_options_changed && app->option_store.dirty)
        {
            app->marker_options_changed = true;   // recorded in case this change brings the core down
            write_running_marker(app);
        }
        app_update_input(app);
        CoreSessionFrame ran = core_session_run_frame(&app->session, &app->shell, &app->cursor_input, game_has_keyboard(app));
        if (ran != CoreSessionFrame::skipped || !game_running(app))
        {
            input_devices_end_frame(&app->input.devices);   // taps seen by a frame (or the UI) are done
        }
        if (ran == CoreSessionFrame::shut_down)
        {
            set_status(app, app->session.message);
            app->menu_visible = true;
            update_settings_context(app);
            remove_running_marker(app);   // the core shut down on its own: a proper end
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
                                                      &recent_picked, app->open_first_menu);
                app->open_first_menu = false;
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

            draw_settings(app);
            // After SETTINGS, so CHOOSE FOLDER opened from it is drawn on top.
            FilePickerChoice picked;
            bool choosing_folder = app->picker.mode == FilePickerMode::folder;
            if (file_picker_draw(&app->picker, &app->catalog, app->pixel_font, app->ui_scale, &app->strings, &picked))
            {
                if (choosing_folder)
                {
                    choose_folder_done(app, &picked);
                }
                else
                {
                    app->pending_start = picked;
                    app->has_pending_start = true;   // see pending_start
                }
            }
            if (!game_running(app))
            {
                app->quick_menu.open = false;
            }
            QuickMenuContext quick = {};
            quick.strings = &app->strings;
            quick.session = &app->session;
            quick.core_options_state = &app->core_options_state;
            quick.core_options = core_options_context(app);
            quick.input = &app->input;
            quick.host = app->session.host;
            quick.settings = &app->settings;
            uint32_t slot_before = app->quick_menu.slot;
            quick.sdl = &app->input_sdl;
            quick.layouts = &app->layouts;
            quick.now_ms = SDL_GetTicks();
            QuickMenuAction quick_action = quick_menu_draw(&app->quick_menu, &quick, app->pixel_font, app->ui_scale);
            draw_remap_prompt(app);
            if (app->quick_menu.slot != slot_before || app->quick_menu.open)
            {
                app->session.state_slot = app->quick_menu.slot;   // F2 / F4 follow the picked slot
            }
            switch (quick_action)
            {
            case QuickMenuAction::save:
                save_state_with_thumbnail(app);
                set_status(app, app->session.message);
                break;
            case QuickMenuAction::load:
                if (core_session_load_state(&app->session))
                {
                    app->quick_menu.open = false;   // back to the game
                }
                set_status(app, app->session.message);
                break;
            case QuickMenuAction::reset:
                libretro_host_reset(app->session.host);
                set_status(app, piccolo_text(&app->strings, Str::status_reset));
                app->quick_menu.open = false;
                break;
            case QuickMenuAction::close_game:
                app->pending_close_game = true;   // the texture may be in this frame already
                app->quick_menu.open = false;
                break;
            case QuickMenuAction::none:
                break;
            }
            CoreOptionsUiContext manager = core_options_context(app);
            core_options_manager_draw(&app->core_options_manager, &app->core_options_state, &manager, app->pixel_font,
                                      app->ui_scale);
            if (app->core_options_manager_was_open && !app->core_options_manager)
            {
                save_core_options(app);   // what was deleted
            }
            app->core_options_manager_was_open = app->core_options_manager;
            InputWindowContext input_context = {};
            input_context.input = &app->input;
            input_context.sdl = &app->input_sdl;
            input_context.strings = &app->strings;
            input_context.settings = &app->settings;
            input_context.core_name = app->core_name;
            input_context.game_name = app->game_name;
            input_context.layouts = &app->layouts;
            input_context.now_ms = SDL_GetTicks();
            bool input_was_open = app->input_window.open;
            input_window_draw(&app->input_window, &input_context, app->pixel_font, app->ui_scale);
            MapWindowContext map_context = {};
            map_context.input = &app->input;
            map_context.strings = &app->strings;
            map_context.layouts = &app->layouts;
            map_context.now_ms = SDL_GetTicks();
            map_window_draw(&app->map_window, &map_context, app->pixel_font, app->ui_scale);
            if (input_was_open && !app->input_window.open)
            {
                save_settings(app);   // SETTINGS tab
                if (app->input_window.dirty && !app->input_config_path.empty())
                {
                    input_config_save(&app->input, app->input_config_path.c_str());   // closed: keep what changed
                    app->input_window.dirty = false;
                }
            }
            if (app->safe_start_open)
            {
                SafeStartChoice answer = safe_start_window_draw(app->pixel_font, app->ui_scale, &app->strings,
                                                                app->crashed_core.c_str(), app->crashed_after_options);
                if (answer == SafeStartChoice::delete_stored)
                {
                    std::vector<std::string> sections;
                    core_options_store_sections_of(&app->option_store, app->crashed_core.c_str(), &sections);
                    for (size_t i = 0; i < sections.size(); i++)
                    {
                        core_options_store_remove_section(&app->option_store, sections[i].c_str());
                    }
                    save_core_options(app);
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Safe start: deleted the stored options of %s",
                                app->crashed_core.c_str());
                }
                if (answer == SafeStartChoice::defaults)
                {
                    app->session.skip_stored_options_once = true;
                }
                if (answer == SafeStartChoice::stored || answer == SafeStartChoice::defaults ||
                    answer == SafeStartChoice::delete_stored)
                {
                    app->pending_start = app->safe_start_pending;   // started at the top of the next loop
                    app->has_pending_start = true;
                    app->safe_start_confirmed = true;
                }
                if (answer != SafeStartChoice::none)
                {
                    app->safe_start_open = false;
                    if (app->net_load_waiting && !app->has_pending_start)
                    {
                        net_answer_load(app, NetContentStatus::failed);   // not started after all
                    }
                }
            }
            if (app->show_debug_window)
            {
                app_draw_debug_ui(app);
            }

            bool ui_up = ui_is_up(app);
            save_after_options_windows(app);
            retro_ui_scrollbars(app->ui_scale);
            retro_ui_cursor(app->ui_scale, ui_up);   // the theme's cursor, on top of everything

            core_session_upload_video(&app->session, &app->shell);
            quick_menu_upload(&app->quick_menu, &app->shell);
            if (app_shell_acquire(&app->shell))
            {
                picture_record(&app->picture, renderer_command_buffer(&app->shell.renderer), &app->session.texture);
            }
            if (!app_shell_present(&app->shell, background))
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
    remove_running_marker(app);   // quit properly (a marker found at launch stays if no game ran)
    game_mode_set(&app->game_mode, false);
    if (app->audio_ready)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    quick_menu_destroy(&app->quick_menu, &app->shell);
    picture_destroy(&app->picture, &app->shell.vulkan);
    if (!app->input_config_path.empty())
    {
        input_config_save(&app->input, app->input_config_path.c_str());
    }
    input_sdl_shutdown(&app->input_sdl);
    net_shutdown_all(app);
    app_shell_destroy(&app->shell);
    delete app;
    return 0;
}
