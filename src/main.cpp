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
#include "bundled_files.h"
#include "frame_delay.h"
#include "game_mode.h"
#include "host_libretro_version.h"
#include "picture.h"
#include "shader_chain.h"
#include "piccolo_version.h"
#include "piccolo_settings.h"
#include "piccolo_text.h"
#include "piccolo_window.h"
#include "quick_menu.h"
#include "recent_games.h"
#include "safe_start_window.h"
#include "shader_check.h"
#include "shader_check_window.h"
#include "text_input_window.h"
#include "settings_window.h"
#include "host_fs.h"
#include "settings_file.h"
#include "core_catalog.h"
#include "core_updater.h"
#include "cores_window.h"
#include "share_window.h"
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
#include "net_service.h"
#include "net_games.h"
#include "net_remote.h"
#include "net_speed.h"
#include "net_trust.h"
#include "net_window.h"
#include "file_picker.h"
#include "headless.h"
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
    std::string net_beacon_applied;      // the beacon server handed to the service ("" = INTERNET off)
    std::string net_share_applied;     // the shared folders the service has (piccolo-share), as one text
    uint32_t    net_share_revision = 0;   // the settings' revision they were looked at for
    bool        net_share_set = false;    // the service has them
    bool       net_streaming = false;    // a device is streaming (remote play): frames and audio are handed over
    // stream_tuning (net_stream.h): what streaming devices asked for, kept only
    // here (never in the settings or the files: a crash leaves nothing changed).
    bool            stream_tuning_set[NET_MAX_PEERS] = {};
    NetStreamTuning stream_tuning_asked[NET_MAX_PEERS];
    uint64_t        stream_tuning_order[NET_MAX_PEERS] = {};   // the newest request wins
    uint64_t        stream_tuning_count = 0;
    bool            stream_tuning_on = false;   // one applies now (`stream_tuning`)
    NetStreamTuning stream_tuning;
    int32_t         stream_tuning_from = -1;    // the peer whose request applies
    AutoFrameDelay  stream_auto_delay = {};     // AUTO for the stream: its own, never saved
    uint64_t        stream_delay_last_ns = 0;
    NetStreamTuning stream_tuning_sent[NET_MAX_PEERS];   // the result each was last told
    bool            stream_tuning_told[NET_MAX_PEERS] = {};
    uint32_t   stream_loop = 0;          // the loop number the stream carries
    uint64_t   stream_serial = 0;        // the core frame last handed over (a repeat isn't sent again)
    NetService net_service;              // the listener on its own thread (net_service.h); its NetAccess:
                                         // net_service.access, used between net_service_lock / _unlock
    InputNet   input_net = {};           // paired devices' pads, in the input router
    NetGames   net_games = {};           // games from devices with FULL access (net_games.h)
    bool       net_load_waiting = false; // a load_request is in pending_start: answered once it ran
    uint32_t   net_load_peer = NET_NO_PEER;
    uint32_t   net_load_id = 0;
    NetRemote  net_remote = {};          // settings and actions from devices (net_remote.h)
    bool       net_remote_ready = false;
    bool       net_paused = false;       // PAUSE from a device
    // PAUSE WHEN CONTROLLERS DISCONNECT: what played last, and the pause it set.
    bool       played_with_gamepad = false;   // the last game input came from a gamepad (not the keyboard)
    bool       disconnect_paused = false;     // START, a controller joining, or closing the menu resumes
    Headless   headless;                      // GO HEADLESS: a streaming device has the game (headless.h)
    bool       menu_was_open = false;         // last loop: for "closing the menu resumes"
    uint32_t   net_paused_by = NET_NO_PEER;   // which: its leaving (or another game) resumes
    char       net_peer_language[NET_MAX_PEERS][8] = {};   // what each device asked for ("" = English)
    bool       net_fast_forward[NET_MAX_PEERS] = {};   // FAST FORWARD held, per peer
    NetRemoteGame net_actions_sent;      // what the action lists last said (sent again when it changes)
    uint64_t   net_controllers_sent[NET_MAX_PEERS] = {};   // a summary of the controllers each device was last sent
    bool       net_devices_wanted[NET_MAX_PEERS] = {};     // asked for `devices` once: gets it again on changes
    uint64_t   net_devices_sent[NET_MAX_PEERS] = {};       // a summary of the `devices` each was last sent
    NetSpeedSender   net_speed_sender;     // a speed test, down (one at a time)
    NetSpeedReceiver net_speed_receiver;   // a speed test, up
    bool             net_speed_reporting = false;   // the sender finished: its report goes out
    NetWindowState net_window_state;   // SETTINGS > NETWORK between frames (OWNER's confirmation)
    std::vector<NetSpeedSummary> net_speed_results;  // what devices' speed tests found (SETTINGS > NETWORK)
    NetAccessLevel   net_actions_level[NET_MAX_PEERS] = {};   // the level each device's action list was made for
    // What the devices were last told runs (game_state): sent again when it changes.
    std::string net_sent_core;           // "" = nothing runs
    std::string net_sent_content;
    FilePicker  picker  = {};
    FilePicker  shader_picker = {};   // QUICK MENU > SHADER > CHOOSE
    SettingLayer shader_picker_scope = SettingLayer::global;   // where its pick goes
    bool        menu_visible = true;   // only hideable while a game runs

    // What the LOAD GAME window picked, started at the top of the NEXT loop:
    // starting stops the old core, which destroys the texture this frame's
    // picture may already have been queued with (a crash at render time).
    FilePickerChoice pending_start = {};
    bool             has_pending_start = false;

    // piccolo's folder, and what was used last (<settings>/last_used.cfg).
    AppPaths    paths = {};
    Picture     picture = {};   // the game on screen (viewport + filter)
    ShaderLibrary shader_library = {};   // librashader, when it's there
    ShaderChain   shader_chain = {};     // pass 0 + the shader preset on the picture (instead of `picture`)
    // piccolo's own presets (shaders/): pass 0, and the final scale per
    // filter (nearest, bilinear, sharp bilinear) when no preset is chosen.
    std::string   shader_pass0_path;
    std::string   shader_presets_folder;   // the curated presets (shaders/presets/): where CHOOSE opens
    std::string   shader_scale_paths[3];
    std::vector<std::string> shader_failed;   // presets that failed: not retried until the setting changes
    std::string   shader_preset_seen;         // the preset setting those failures are for
    // A preset just applied that's too slow asks KEEP / REVERT
    // (shader_check.h). REVERT puts the preset setting of every layer
    // (global, core, game) back as it was before the change.
    ShaderCheck   shader_check = {};
    uint64_t      shader_check_asked_ms = 0;
    uint32_t      shader_layers_start = UINT32_MAX;   // game_starts the layers below are from
    bool          shader_layer_set[3] = {};           // the preset setting in each layer now
    std::string   shader_layer_value[3];
    bool          shader_revert_set[3] = {};          // ... and before the change being checked
    std::string   shader_revert_value[3];
    bool          shader_reverted = false;            // the next change is REVERT's: not checked
    float         pass0_applied[PICCOLO_PASS0_CONTROL_COUNT] = {};   // what pass 0's parameters hold
    float         profile_applied[PICCOLO_PROFILE_PARAM_COUNT] = {};  // the same for the color profile's
    bool          pass0_stale = true;         // set them all (pass 0 was just loaded)
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
    // A core's TEXT request (text_input_window.h): open while it's pending.
    TextInputWindow  text_input_window = {};

    bool             safe_start_open = false;
    bool             safe_start_confirmed = false;    // the next start skips the check (answered)
    FilePickerChoice safe_start_pending = {};

    // The core options UI's state, shared by the quick menu (live options)
    // and the CORE OPTIONS MANAGER window (CONFIG menu).
    CoreOptionsUiState core_options_state;
    bool               core_options_manager = false;   // the window is open
    bool               core_options_manager_was_open = false;

    // TOOLS > CORES: the core downloader (core_updater.h) and its window. Set
    // up when the window opens (the server and cores folder may have changed).
    CoreUpdater        core_updater;
    CoresWindow        cores_window;
    ShareWindow        share_window;     // TOOLS > SHARE (files with another piccolo at OWNER level)
    NetShareFolders    share_folders;    // this piccolo's shared folders: game folders, saves, states
    NetShareLink       share_link;       // SHARE's connection to another piccolo, as its client
    NetShareLinkStatus share_status;     // read once per loop while SHARE is open
    bool               share_connected = false;   // last loop: connected with OWNER access
    std::vector<NetShareClientDone> share_done;
    uint32_t           share_rate_id = 0;    // the copy the rate is measured for
    uint64_t           share_rate_done = 0;
    uint64_t           share_rate_ticks = 0;
    uint32_t           share_rate_kbps = 0;

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

#if defined(SDL_PLATFORM_ANDROID)
    // Android: cores load only from the app's private storage (shared storage
    // is noexec), so the CORES setting isn't offered (settings_window.cpp).
    std::string cores = app->paths.cores;
#else
    std::string cores = folder_setting(app, PICCOLO_PATH_CORES, app->paths.cores);
#endif
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

// The running core asked for a string (TEXT): its box is up, the game paused.
static bool text_input_pending(const App* app)
{
    return game_running(app) && app->session.host->text_input.mode == RETRO_EXT_TEXT_INPUT_TEXT;
}

static void set_status(App* app, const char* text)
{
    std::snprintf(app->status, sizeof(app->status), "%s", text);
}

// The game pauses while the menu or the LOAD GAME window is up, and its
// queued audio is dropped so resuming doesn't play stale sound.
static void update_pause(App* app)
{
    bool menu_open = app->menu_visible || app->picker.open || app->shader_picker.open || app->settings_window.open ||
                     app->settings_window.image_open || app->quick_menu.open || app->core_options_manager ||
                     app->input_window.open || map_window_open_now(&app->map_window);
    // The text box always pauses (the core's text isn't typed into a running game).
    bool pause_for_menu = (menu_open && settings_get_bool(&app->settings, PICCOLO_PAUSE_IN_MENU)) || app->remap_prompt ||
                          text_input_pending(app);
    if (app->headless.on)
    {
        pause_for_menu = text_input_pending(app);   // nobody sees the menus here; the core's text box still waits
    }
    // Not while a device streams: whoever plays is looking at the phone, not
    // at this window.
    bool pause_for_focus = !app->window_focused && !app->net_streaming &&
                           settings_get_bool(&app->settings, PICCOLO_PAUSE_ON_FOCUS_LOSS);
    bool pause = game_running(app) && (pause_for_menu || pause_for_focus || app->net_paused || app->disconnect_paused);
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
// A learned AUTO latency at or above this is taken for an anomaly, not this
// core's need, and learned again (AUTO settles around 50 ms on the RK3588,
// 112 ms at worst without GameMode).
constexpr uint32_t LEARNED_LATENCY_ANOMALY_MS = 120;

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
        if (app->learned_start_ms >= LEARNED_LATENCY_ANOMALY_MS)
        {
            // An anomaly (a session that struggled: a heavy shader in a debug
            // build, a busy machine): learned again from AUTO's own start.
            SDL_Log("Audio latency: learned %u ms for %s ignored (%u ms or more), learning again",
                    app->learned_start_ms, app->core_name, LEARNED_LATENCY_ANOMALY_MS);
            app->learned_start_ms = 0;
        }
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

// The running core's file name ("snes9x_libretro.so"), "" when no game runs:
// the core downloader never replaces or removes it.
static std::string running_core_file(const App* app)
{
    if (!game_running(app))
    {
        return std::string();
    }
    const char* slash = std::strrchr(app->last_core, '/');
    return slash != nullptr ? std::string(slash + 1) : std::string(app->last_core);
}

// TOOLS > CORES: the downloader is set up again on every open (unless it's
// busy), so a changed CORE SERVER or cores folder applies.
static void open_cores_window(App* app)
{
    if (app->core_updater.state == CoreUpdaterState::idle && !app->paths.root.empty())
    {
        const char* keys[CORE_UPDATER_MAX_KEYS] = {};
        int32_t key_count = core_updater_key_count();
        key_count = key_count < CORE_UPDATER_MAX_KEYS ? key_count : CORE_UPDATER_MAX_KEYS;
        for (int32_t i = 0; i < key_count; i++)
        {
            keys[i] = core_updater_key_hex(i);
        }
        std::string work = app->paths.cache + "/downloads";
        core_updater_init(&app->core_updater, "piccolo/" PICCOLO_VERSION,
                          settings_get_text(&app->settings, PICCOLO_CORE_SERVER),
                          settings_get_bool(&app->settings, PICCOLO_CORE_SERVER_INSECURE), keys, key_count,
                          core_index_build_target(), app->cores_directory.c_str(), work.c_str());
    }
    cores_window_open(&app->cores_window, &app->core_updater);
}

// What this piccolo shares with OWNER devices (and lists on SHARE's own
// side): the game folders, SAVES and STATES as the settings say now.
static void share_folder_paths(const App* app, std::vector<std::string>* game_folders, std::string* saves,
                               std::string* states)
{
    piccolo_settings_game_folders(&app->settings, game_folders);
    *saves = folder_setting(app, PICCOLO_PATH_SAVES, app->paths.saves);
    *states = folder_setting(app, PICCOLO_PATH_STATES, app->paths.states);
}

static bool net_prepare(App* app);

// TOOLS > SHARE: this piccolo's shared folders as they are now, and the
// connection to another piccolo (started the first time, kept afterwards so
// copies go on with the window closed).
static void open_share_window(App* app)
{
    std::vector<std::string> game_folders;
    std::string saves;
    std::string states;
    share_folder_paths(app, &game_folders, &saves, &states);
    net_share_set_folders(&app->share_folders, &game_folders, saves.c_str(), states.c_str());
    if (!net_share_link_started(&app->share_link) && net_prepare(app))
    {
        std::string hosts_path = app->paths.settings + "/net_hosts.cfg";
        net_share_link_start(&app->share_link, &app->net_identity, settings_get_text(&app->settings, PICCOLO_NET_NAME),
                             hosts_path.c_str());
    }
    if (net_share_link_started(&app->share_link))
    {
        net_share_link_set_folders(&app->share_link, &game_folders, saves.c_str(), states.c_str());
    }
    share_window_open(&app->share_window);
}

// What a copy that didn't go well says on SHARE's bottom line.
static Str share_done_text(NetShareStatus status)
{
    switch (status)
    {
    case NetShareStatus::exists:
        return Str::share_done_exists;
    case NetShareStatus::no_space:
        return Str::share_done_no_space;
    case NetShareStatus::mismatch:
        return Str::share_done_mismatch;
    case NetShareStatus::refused:
    case NetShareStatus::not_found:
        return Str::share_done_refused;
    case NetShareStatus::busy:
        return Str::share_done_busy;
    default:
        return Str::share_done_failed;
    }
}

// SHARE, once per loop while the window is open or the connection runs:
// the window's clicks go to the connection, its answers to the window.
static void share_update(App* app, ShareTransferLine* line)
{
    ShareWindow* window = &app->share_window;
    NetShareLink* link = &app->share_link;
    if (!net_share_link_started(link))
    {
        window->connect = false;
        window->disconnect = false;
        window->copies.clear();
        window->cancel = false;
        return;
    }
    if (window->open)
    {
        net_share_link_status(link, &app->share_status);
    }
    if (window->connect)
    {
        window->connect = false;
        if (window->found_selected >= 0 && static_cast<size_t>(window->found_selected) < app->share_status.found.size())
        {
            net_share_link_connect(link, app->share_status.found[static_cast<size_t>(window->found_selected)].address,
                                   window->code);
        }
    }
    if (window->disconnect)
    {
        window->disconnect = false;
        net_share_link_disconnect(link);
    }
    bool connected = app->share_status.state == NetShareLinkState::ready &&
                     app->share_status.access == NetAccessLevel::owner;
    if (connected != app->share_connected)
    {
        app->share_connected = connected;
        share_window_remote_reset(window);   // a new piccolo, or none: its side starts over
        window->code[0] = '\0';             // used (or not needed any more)
    }
    std::string path;
    if (connected && share_window_remote_wants_list(window, &path))
    {
        net_share_link_list(link, path.c_str());
    }
    NetShareStatus status = NetShareStatus::ok;
    std::vector<NetShareEntry> entries;
    if (connected && net_share_link_listing(link, &path, &status, &entries))
    {
        share_window_set_remote_list(window, path.c_str(), status, &entries);
    }
    for (size_t i = 0; i < window->copies.size(); i++)
    {
        const ShareCopy* copy = &window->copies[i];
        const char* local = copy->to_remote ? copy->from.c_str() : copy->to.c_str();
        const char* remote = copy->to_remote ? copy->to.c_str() : copy->from.c_str();
        net_share_link_copy(link, copy->to_remote, local, remote, copy->size, 0);
        window->message.clear();
    }
    window->copies.clear();
    if (window->cancel)
    {
        window->cancel = false;
        net_share_link_cancel(link);
    }
    net_share_link_take_done(link, &app->share_done);
    for (size_t i = 0; i < app->share_done.size(); i++)
    {
        const NetShareClientDone* done = &app->share_done[i];
        if (done->status == NetShareStatus::ok)
        {
            window->local.stale = true;   // a new file on one side
            window->remote.stale = true;
            continue;
        }
        const std::string* name = done->copy.to_remote ? &done->copy.local : &done->copy.remote;
        window->message = std::string(piccolo_text(&app->strings, share_done_text(done->status))) + ": " + *name;
    }

    // The bottom line, with the rate over about a second.
    NetShareProgress progress;
    NetShareClientCopy copy;
    uint32_t queued = 0;
    net_share_link_progress(link, &progress, &copy, &queued);
    line->active = progress.role != NetShareRole::none;
    if (line->active)
    {
        uint64_t now = SDL_GetTicks();
        if (progress.id != app->share_rate_id)
        {
            app->share_rate_id = progress.id;
            app->share_rate_done = progress.done;
            app->share_rate_ticks = now;
            app->share_rate_kbps = 0;
        }
        else if (now - app->share_rate_ticks >= 1000)
        {
            uint64_t bytes = progress.done - app->share_rate_done;
            app->share_rate_kbps = static_cast<uint32_t>(bytes * 8 / (now - app->share_rate_ticks));
            app->share_rate_done = progress.done;
            app->share_rate_ticks = now;
        }
        size_t slash = copy.remote.rfind('/');
        line->name = slash == std::string::npos ? copy.remote : copy.remote.substr(slash + 1);
        line->sending = copy.to_remote;
        line->done = progress.done;
        line->size = progress.size;
        line->rate_kbps = app->share_rate_kbps;
        line->queued = queued;
    }
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
        case MenuAction::cores:
            open_cores_window(app);
            break;
        case MenuAction::share:
            open_share_window(app);
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

// Does a streaming device's tuning decide the frame delay now (not "host")?
static bool stream_delay_overrides(const App* app)
{
    return app->stream_tuning_on && app->stream_tuning.delay != NetStreamDelay::host;
}

// The frame delay a streaming device asked for (stream_tuning): OFF, N ms or
// AUTO with its own search. Only in memory: nothing is saved, and the game's
// local AUTO isn't fed (its minute starts over, as for a pause).
static void stream_frame_delay(App* app)
{
    auto_frame_delay_interrupt(&app->auto_delay);
    app->frame_delay_ms = 0;
    bool applies = game_running(app) && !app->session.paused && !core_session_fast_forwarding(&app->session) &&
                   app->session.timing.effective == TimingMode::vsync &&
                   app->stream_tuning.delay != NetStreamDelay::off;
    if (!applies)
    {
        app->stream_delay_last_ns = 0;
        auto_frame_delay_interrupt(&app->stream_auto_delay);
        return;
    }
    uint32_t use = app->stream_tuning.delay_ms;
    if (app->stream_tuning.delay == NetStreamDelay::automatic)
    {
        // The player is on the phone: focus doesn't matter here, the menu does.
        if (app->menu_visible || app->picker.open || app->settings_window.open || app->settings_window.image_open)
        {
            auto_frame_delay_interrupt(&app->stream_auto_delay);
        }
        uint64_t now = SDL_GetTicksNS();
        double elapsed =
            app->stream_delay_last_ns > 0 ? static_cast<double>(now - app->stream_delay_last_ns) / 1e9 : 0.0;
        app->stream_delay_last_ns = now;
        use = auto_frame_delay_update(&app->stream_auto_delay, app->session.timing.missed, elapsed,
                                      auto_frame_delay_start_ms(app->session.timing.display_hz));
    }
    app->frame_delay_ms = use;
    if (use > 0)
    {
        SDL_DelayPrecise(static_cast<Uint64>(use) * 1000000u);
    }
}

static void frame_delay(App* app)
{
    if (stream_delay_overrides(app))
    {
        stream_frame_delay(app);
        return;
    }
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

// The slow shader question is up (shader_check.h).
static bool shader_check_asking(const App* app)
{
    return app->shader_check.state == ShaderCheckState::asking;
}

constexpr SettingLayer SHADER_LAYERS[3] = {SettingLayer::global, SettingLayer::core, SettingLayer::game};

// The preset setting as each layer holds it now (absent or a value).
static void shader_layers_read(const App* app, bool set[3], std::string value[3])
{
    for (uint32_t i = 0; i < 3; i++)
    {
        const char* text = settings_get_layer(&app->settings, SHADER_LAYERS[i], PICCOLO_VIDEO_SHADER_PRESET);
        set[i] = text != nullptr;
        value[i] = text != nullptr ? text : "";
    }
}

// KEEP: nothing changes. REVERT: every layer's preset setting as before the
// change, saved. Either way the check is over until the next change.
static void shader_check_answer(App* app, ShaderCheckChoice choice)
{
    shader_check_stop(&app->shader_check);
    if (choice != ShaderCheckChoice::revert)
    {
        return;
    }
    for (uint32_t i = 0; i < 3; i++)
    {
        if (app->shader_revert_set[i])
        {
            settings_set(&app->settings, SHADER_LAYERS[i], PICCOLO_VIDEO_SHADER_PRESET, app->shader_revert_value[i].c_str());
        }
        else
        {
            settings_reset(&app->settings, SHADER_LAYERS[i], PICCOLO_VIDEO_SHADER_PRESET);
        }
    }
    app->shader_reverted = true;
    save_settings(app);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Shader preset too slow: reverted");
}

// Once per loop that ran core frames: times the loop for the check. The
// target is a display refresh in Vsync timing (each loop presents one), a
// core frame in Exact. Fast forward is fast on purpose: not a sample.
static void measure_shader_check(App* app)
{
    const FrameTiming* timing = &app->session.timing;
    double hz = timing->effective == TimingMode::vsync ? timing->display_hz : timing->core_fps;
    if (hz <= 0.0)
    {
        return;
    }
    bool sample = !core_session_fast_forwarding(&app->session);
    if (shader_check_loop(&app->shader_check, sample, SDL_GetTicksNS(), 1e9 / hz))
    {
        app->shader_check_asked_ms = SDL_GetTicks();
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Shader preset too slow: %.1f fps, target %.1f",
                    1e9 / app->shader_check.measured_ns, hz);
    }
}

// A window is open over the menu bar (or the game).
static bool any_window_open(const App* app)
{
    return app->picker.open || app->shader_picker.open || shader_check_asking(app) || app->settings_window.open ||
           app->settings_window.image_open ||
           app->quick_menu.open || app->core_options_manager || app->cores_window.open || app->share_window.open ||
           app->safe_start_open ||
           app->show_debug_window || app->input_window.open ||
           map_window_open_now(&app->map_window) || text_input_pending(app);
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
        if (text_input_pending(app))
        {
            libretro_host_text_input_answer(app->session.host, nullptr);   // the core's box is on top: cancelled
        }
        else if (shader_check_asking(app))
        {
            shader_check_answer(app, ShaderCheckChoice::revert);   // on top of everything: Esc reverts
        }
        else if (map_window_open_now(&app->map_window))
        {
            map_window_close(&app->map_window, &app->input);
        }
        else if (app->shader_picker.open)   // opened from the QUICK MENU: closes before it
        {
            app->shader_picker.open = false;
        }
        else if (app->quick_menu.open)
        {
            app->quick_menu.open = false;
        }
        else if (app->core_options_manager)
        {
            app->core_options_manager = false;
        }
        else if (app->cores_window.open)
        {
            app->cores_window.open = false;
        }
        else if (app->share_window.open)
        {
            app->share_window.open = false;
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

// Is a connected gamepad (local or remote) on any port?
static bool gamepad_on_a_port(const App* app)
{
    for (size_t c = 0; c < app->input.controllers.size(); c++)
    {
        const InputDevice* device = input_devices_find_const(&app->input.devices, app->input.controllers[c].device_id);
        if (device != nullptr && device->connected && device->kind == InputDeviceKind::gamepad &&
            input_controller_port(&app->input, static_cast<uint32_t>(c)) != INPUT_NO_PORT)
        {
            return true;
        }
    }
    return false;
}

// PAUSE WHEN CONTROLLERS DISCONNECT: notes whether a gamepad or the keyboard
// played last (a RetroPad input held in the game), and resumes on START.
static void follow_what_plays(App* app, const InputFrame* frame)
{
    uint32_t pad_actions = static_cast<uint32_t>(InputAction::pad_turbo);   // the RetroPad's buttons and sticks
    for (size_t c = 0; c < app->input.controllers.size(); c++)
    {
        const InputController* controller = &app->input.controllers[c];
        if (controller->layer != InputLayer::game)
        {
            continue;
        }
        bool playing = false;
        for (uint32_t a = 0; a < pad_actions && !playing; a++)
        {
            playing = controller->held_last[a];
        }
        const InputDevice* device = input_devices_find_const(&app->input.devices, controller->device_id);
        if (playing && device != nullptr)
        {
            app->played_with_gamepad = device->kind == InputDeviceKind::gamepad;
        }
    }
    // Closing the menu (or any window) resumes too: with every pad gone and
    // the keyboard on no port, nothing else could (Andrés).
    bool menu_open = app->menu_visible || any_window_open(app);
    bool menu_closed = app->menu_was_open && !menu_open;
    app->menu_was_open = menu_open;
    if (!app->disconnect_paused)
    {
        return;
    }
    bool start = frame->joined_port != INPUT_NO_PORT;
    for (uint32_t p = 0; p < INPUT_PORT_COUNT && !start; p++)
    {
        start = (frame->pad_buttons[p] & (1u << 3)) != 0;   // RETRO_DEVICE_ID_JOYPAD_START
    }
    if (start || menu_closed || !game_running(app))
    {
        app->disconnect_paused = false;
        if (start || menu_closed)
        {
            set_status(app, piccolo_text(&app->strings, Str::status_resumed));
            SDL_Log("Resumed: %s", start ? "START" : "the menu closed");
        }
    }
}

static void app_update_input(App* app)
{
    bool ui_up = ui_is_up(app);
    app->input.settings.ui_active = ui_up;   // the INTERFACE layer
    piccolo_settings_input(&app->settings, &app->input.settings);   // SETTINGS > INPUT, per core / game too
    text_input_set_osk(&app->shell.text_input, piccolo_settings_osk(&app->settings) ? TextInputOsk::on : TextInputOsk::off);
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
        // No CONTROLS here (Andrés): joining again is just START. The game
        // pauses if that was the last gamepad playing (the setting).
        if (game_running(app) && settings_get_bool(&app->settings, PICCOLO_PAUSE_ON_DISCONNECT) &&
            app->played_with_gamepad && !gamepad_on_a_port(app))
        {
            app->disconnect_paused = true;
            set_status(app, piccolo_text(&app->strings, Str::status_paused_disconnected));
            SDL_Log("Paused: the last gamepad playing went away");
        }
    }
    follow_what_plays(app, &frame);
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

// Loads what each shader stage should hold when it changes: stage 0 = pass 0,
// stage 1 = the preset setting, or piccolo's final scale for `filter` when
// there's none (or it failed). Then pass 0's parameters from the settings,
// only the ones that changed. A preset that fails isn't retried every frame.
static bool shader_failed_before(const App* app, const std::string& path)
{
    for (size_t i = 0; i < app->shader_failed.size(); i++)
    {
        if (app->shader_failed[i] == path)
        {
            return true;
        }
    }
    return false;
}

static void load_shader_stage(App* app, uint32_t stage, const std::string& path)
{
    if (app->shader_chain.stages[stage].preset_path == path || shader_failed_before(app, path))
    {
        return;
    }
    if (path.empty())
    {
        shader_chain_clear(&app->shader_chain, &app->shader_library, &app->shell.vulkan, stage);
    }
    else if (!shader_chain_load(&app->shader_chain, &app->shader_library, &app->shell.vulkan, stage, path.c_str()))
    {
        app->shader_failed.push_back(path);
    }
    if (stage == 0)
    {
        app->pass0_stale = true;
    }
}

static void update_shader_stages(App* app, ViewportFilter filter)
{
    if (!app->shader_library.loaded)
    {
        return;
    }
    const char* preset = settings_get_text(&app->settings, PICCOLO_VIDEO_SHADER_PRESET);
    // A preset applied while this game runs is checked for speed; the one a
    // game starts with (its core / game layers) isn't: that's not a change
    // made now, and there's nothing to revert to.
    bool new_game = app->shader_layers_start != app->game_starts;
    if (new_game || app->shader_preset_seen != preset)
    {
        if (!new_game && preset[0] != '\0' && !app->shader_reverted)
        {
            // Changed again while a check runs: REVERT still goes back to
            // before the first change (the last preset that was fine).
            if (app->shader_check.state == ShaderCheckState::idle)
            {
                for (uint32_t i = 0; i < 3; i++)
                {
                    app->shader_revert_set[i] = app->shader_layer_set[i];
                    app->shader_revert_value[i] = app->shader_layer_value[i];
                }
            }
            shader_check_start(&app->shader_check);
        }
        else
        {
            shader_check_stop(&app->shader_check);
        }
        shader_layers_read(app, app->shader_layer_set, app->shader_layer_value);
        app->shader_layers_start = app->game_starts;
        app->shader_reverted = false;
    }
    if (app->shader_preset_seen != preset)
    {
        app->shader_preset_seen = preset;   // a new choice: everything may be tried again
        app->shader_failed.clear();
    }
    load_shader_stage(app, 0, app->shader_pass0_path);
    const std::string* scale = &app->shader_scale_paths[static_cast<uint32_t>(filter)];
    if (preset[0] != '\0' && !shader_failed_before(app, app->shader_preset_seen))
    {
        load_shader_stage(app, 1, app->shader_preset_seen);
    }
    else
    {
        load_shader_stage(app, 1, *scale);
    }

    if (app->shader_chain.stages[0].chain == nullptr)
    {
        return;
    }
    for (uint32_t i = 0; i < PICCOLO_PASS0_CONTROL_COUNT; i++)
    {
        float value = static_cast<float>(settings_get_real(&app->settings, PICCOLO_PASS0_CONTROLS[i].key));
        if (app->pass0_stale || value != app->pass0_applied[i])
        {
            shader_chain_set_param(&app->shader_chain, &app->shader_library, 0, PICCOLO_PASS0_CONTROLS[i].param, value);
            app->pass0_applied[i] = value;
        }
    }
    float profile[PICCOLO_PROFILE_PARAM_COUNT] = {};
    piccolo_color_profile_params(&app->settings, profile);
    for (uint32_t i = 0; i < PICCOLO_PROFILE_PARAM_COUNT; i++)
    {
        if (app->pass0_stale || profile[i] != app->profile_applied[i])
        {
            shader_chain_set_param(&app->shader_chain, &app->shader_library, 0, PICCOLO_PROFILE_PARAM_NAMES[i], profile[i]);
            app->profile_applied[i] = profile[i];
        }
    }
    app->pass0_stale = false;
}

static void draw_game(App* app)
{
    CoreSession* session = &app->session;
    app->picture.ready = false;
    app->shader_chain.ready = false;
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

    update_shader_stages(app, input.filter);

    ViewportResult viewport = {};
    bool shaded = false;
    bool shown = false;
    if (viewport_compute(&input, &viewport))
    {
        shaded = shader_chain_prepare(&app->shader_chain, &app->shell.vulkan, &session->texture, &viewport);
        shown = shaded || picture_prepare(&app->picture, &app->shell.vulkan, &session->texture, &viewport);
    }
    if (shaded)
    {
        app->picture.ready = false;   // the chain draws it; the picture records nothing
    }
    if (shown)
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
        if (shaded)
        {
            shader_chain_draw(&app->shader_chain, ImGui::GetBackgroundDrawList(), 0.0f, 0.0f);
        }
        else
        {
            picture_draw(&app->picture, ImGui::GetBackgroundDrawList(), &session->texture, session->texture_id, 0.0f,
                         0.0f);
        }
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

// QUICK MENU > SHADER > CHOOSE: the picker opens in the
// chosen preset's folder, else in the curated presets. What's picked goes to
// `scope`, the scope the QUICK MENU's SHADER tab edits.
static void open_shader_picker(App* app, SettingLayer scope)
{
    std::string folder = settings_get_at(&app->settings, scope, PICCOLO_VIDEO_SHADER_PRESET, nullptr);
    size_t slash = folder.find_last_of("/\\");
    folder = slash == std::string::npos ? app->shader_presets_folder : folder.substr(0, slash);
    if (!host_fs_exists(folder.c_str()))
    {
        folder = app->shader_presets_folder;
    }
    app->shader_picker_scope = scope;
    file_picker_open_shader(&app->shader_picker, folder.c_str());
}

// Drawn over the QUICK MENU.
static void draw_shader_picker(App* app)
{
    FilePickerChoice choice = {};
    if (!file_picker_draw(&app->shader_picker, &app->catalog, app->pixel_font, app->ui_scale, &app->strings, &choice))
    {
        return;
    }
    // NONE: no preset at this scope. Globally that's the default; a core or
    // game says it explicitly, so a global preset doesn't apply there.
    SettingLayer scope = app->shader_picker_scope;
    if (choice.content_path[0] != '\0' || scope != SettingLayer::global)
    {
        settings_set(&app->settings, scope, PICCOLO_VIDEO_SHADER_PRESET, choice.content_path);
    }
    else
    {
        settings_reset(&app->settings, scope, PICCOLO_VIDEO_SHADER_PRESET);
    }
    save_settings(app);
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
        // Pass 0's result (the shown part, after the crop): drawn over the
        // preview so the picture controls show. Only once the chain has
        // written it (a new image is still UNDEFINED while the UI is built).
        const ShaderChain* chain = &app->shader_chain;
        if (chain->ready && chain->stages[0].chain != nullptr && chain->middle.texture_id != 0 &&
            chain->middle.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            image.processed_texture_id = chain->middle.texture_id;
            image.processed_width = chain->middle.width;
            image.processed_height = chain->middle.height;
        }
    }
    image.controls_available = app->shader_library.loaded;
    SettingsWindowContext context = {};
    context.settings = &app->settings;
    context.paths = &app->paths;
    context.core_name = app->core_name;
    context.game_name = app->game_name;
    context.image = &image;
    context.strings = &app->strings;
    context.themes = &app->themes;
    NetWindowContext network;
    network.state = &app->net_window_state;
    network.settings = &app->settings;
    network.strings = &app->strings;
    network.access = app->net_running ? &app->net_service.access : nullptr;
    network.listen_failed = app->net_listen_failed;
    network.this_device = app->net_fingerprint;
    network.speed = &app->net_speed_results;
    network.internet = settings_get_bool(&app->settings, PICCOLO_NET_INTERNET);
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
    // RUN-AHEAD: what the running core does with it.
    char run_ahead_text[200] = {};
    const LibretroHost* run_ahead_host = app->session.host;
    if (!game_running(app))
    {
        std::snprintf(run_ahead_text, sizeof(run_ahead_text), "%s", piccolo_text(&app->strings, Str::run_ahead_no_game));
    }
    else if (run_ahead_host->core_run_ahead_max == 0)
    {
        std::snprintf(run_ahead_text, sizeof(run_ahead_text), "%s", piccolo_text(&app->strings, Str::run_ahead_none));
    }
    else
    {
        std::snprintf(run_ahead_text, sizeof(run_ahead_text), "%s: %u (%s %u)",
                      piccolo_text(&app->strings, Str::run_ahead_in_use), libretro_host_run_ahead_applied(run_ahead_host),
                      piccolo_text(&app->strings, Str::run_ahead_most), run_ahead_host->core_run_ahead_max);
    }
    context.run_ahead = run_ahead_text;
    context.frame_delay_found = game_running(app) && auto_delay->locked;
    // The NETWORK tab reads and changes the NetAccess (pairing, levels,
    // FORGET): the network thread waits meanwhile.
    bool network_locked = app->net_running;
    if (network_locked)
    {
        net_service_lock(&app->net_service);
    }
    settings_window_draw(&app->settings_window, &context, app->pixel_font, app->ui_scale);
    if (network_locked)
    {
        net_service_unlock(&app->net_service);
    }
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
        if ((peer == NET_NO_PEER || peer == i) && app->net_service.access.peers[i].state == NetAccessPeerState::authorized)
        {
            net_access_send(&app->net_service.access, i, NetChannel::control, app->net_games.reply.data(), size);
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
    int32_t device = app->net_service.access.peers[peer].device;
    if (device < 0 || static_cast<size_t>(device) >= app->net_service.access.trust.devices.size())
    {
        return NetAccessLevel::gamepad;
    }
    return app->net_service.access.trust.devices[static_cast<size_t>(device)].access;
}

// GO HEADLESS (headless.h, net_stream.h): the state, to every authorized
// device that may stream (each learns whether it's the one holding it).
static void headless_tell(App* app)
{
    for (uint32_t i = 0; i < NET_MAX_PEERS; i++)
    {
        if (app->net_service.access.peers[i].state != NetAccessPeerState::authorized ||
            !net_access_allows(net_peer_access(app, i), NetAccessLevel::stream))
        {
            continue;
        }
        uint8_t out[4];
        size_t size = net_stream_write_headless_state(out, sizeof(out), app->headless.on,
                                                      app->headless.on && app->headless.peer == i);
        net_access_send(&app->net_service.access, i, NetChannel::control, out, size);
    }
}

// Back on piccolo's window. `pause`: the device that had the game is gone
// (or stopped streaming), so nobody plays it: paused until someone here does.
static void headless_back(App* app, bool pause)
{
    if (!app->headless.on)
    {
        return;
    }
    headless_leave(&app->headless, app->shell.window);
    if (pause && game_running(app))
    {
        app->disconnect_paused = true;
        set_status(app, piccolo_text(&app->strings, Str::status_headless_paused));
    }
    else
    {
        set_status(app, piccolo_text(&app->strings, Str::status_headless_back));
    }
    SDL_Log("Headless: back on the window%s", pause ? " (paused)" : "");
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
        if ((peer == NET_NO_PEER || peer == i) && app->net_service.access.peers[i].state == NetAccessPeerState::authorized)
        {
            app->net_actions_level[i] = net_peer_access(app, i);
            size_t size = net_remote_write_actions(&app->net_remote, id, app->net_peer_language[i],
                                                   app->paths.root.c_str(), app->net_actions_level[i], &game);
            net_access_send(&app->net_service.access, i, NetChannel::control, app->net_remote.reply.data(), size);
        }
    }
}

// The ports the core has, as the quick menu's CONTROLS shows them: as many
// as SETTINGS > INPUT > PORTS says, no more than the core describes. 0 when
// no game runs.
static uint32_t net_core_ports(const App* app)
{
    const LibretroHost* host = app->session.host;
    if (!game_running(app) || host == nullptr)
    {
        return 0;
    }
    int32_t wanted = settings_get_int(&app->settings, PICCOLO_INPUT_PORTS);
    uint32_t ports = wanted >= 1 && wanted <= static_cast<int32_t>(INPUT_PORT_COUNT) ? static_cast<uint32_t>(wanted)
                                                                                     : INPUT_PORT_COUNT;
    if (host->input_info.port_count > 0 && host->input_info.port_count < ports)
    {
        ports = host->input_info.port_count;
    }
    return ports;
}

// The port a peer's pad plays on, or -1.
static int32_t net_pad_port(const App* app, uint32_t peer, uint32_t pad)
{
    const InputNetPad* entry = &app->input_net.pads[peer][pad];
    if (!entry->active)
    {
        return -1;
    }
    int32_t controller = input_first_controller(&app->input, entry->device);
    return controller < 0 ? -1 : input_controller_port(&app->input, static_cast<uint32_t>(controller));
}

// What the core expects on each port, and where this peer's pads play.
static void net_controllers_for(const App* app, uint32_t peer, NetControllers* controllers)
{
    for (uint32_t pad = 0; pad < NET_CONTROLLERS_PADS; pad++)
    {
        controllers->pad_ports[pad] = static_cast<int8_t>(net_pad_port(app, peer, pad));
    }
    controllers->ports.clear();
    uint32_t ports = net_core_ports(app);
    const LibretroHost* host = app->session.host;
    for (uint32_t p = 0; p < ports; p++)
    {
        NetPort port;
        port.device = host->port_devices[p];
        const LibretroInputInfo* info = &host->input_info;
        if (p < info->port_count)
        {
            for (uint32_t t = 0; t < info->ports[p].count; t++)
            {
                NetPortDevice device;
                device.id = info->ports[p].types[t].id;
                device.name = info->ports[p].types[t].description;
                port.devices.push_back(device);
            }
        }
        for (uint32_t d = 0; d < info->descriptor_count; d++)
        {
            const LibretroInputDescriptor* in = &info->descriptors[d];
            if (in->port != p)
            {
                continue;
            }
            NetPortDescriptor descriptor;
            descriptor.device = static_cast<uint8_t>(in->device);
            descriptor.index = static_cast<uint8_t>(in->index);
            descriptor.id = static_cast<uint8_t>(in->id);
            descriptor.name = in->description;
            port.descriptors.push_back(descriptor);
        }
        controllers->ports.push_back(port);
    }
}

// A summary of what net_controllers_for would say, compared every loop
// (no allocation) to know when to send it again.
static uint64_t net_controllers_summary(const App* app, uint32_t peer)
{
    uint64_t summary = 1469598103934665603ull;   // FNV-1a over the numbers
    uint32_t values[16] = {};
    uint32_t count = 0;
    for (uint32_t pad = 0; pad < NET_CONTROLLERS_PADS; pad++)
    {
        values[count++] = static_cast<uint32_t>(net_pad_port(app, peer, pad) + 1);
    }
    uint32_t ports = net_core_ports(app);
    values[count++] = ports;
    if (ports > 0)
    {
        values[count++] = app->session.host->input_info.controller_serial;
        values[count++] = app->session.host->input_info.descriptor_serial;
        for (uint32_t p = 0; p < ports && count < 16; p++)
        {
            values[count++] = app->session.host->port_devices[p];
        }
    }
    for (uint32_t i = 0; i < count; i++)
    {
        summary = (summary ^ values[i]) * 1099511628211ull;
    }
    return summary;
}

static void net_send_controllers(App* app, uint32_t peer, uint32_t id)
{
    NetControllers controllers;
    net_controllers_for(app, peer, &controllers);
    size_t size = net_settings_write_controllers(app->net_remote.reply.data(), app->net_remote.reply.size(), id,
                                                 &controllers);
    net_access_send(&app->net_service.access, peer, NetChannel::control, app->net_remote.reply.data(), size);
    app->net_controllers_sent[peer] = net_controllers_summary(app, peer);
}

// port_device: the device on the port this peer's pad plays on, as the
// quick menu's CONTROLS changes it (the core is told; kept for the core).
static void net_change_port_device(App* app, uint32_t peer, uint8_t pad, uint32_t device_id, const char* device)
{
    int32_t port = net_pad_port(app, peer, pad);
    const LibretroHost* host = app->session.host;
    if (port < 0 || static_cast<uint32_t>(port) >= net_core_ports(app) ||
        static_cast<uint32_t>(port) >= host->input_info.port_count)
    {
        return;   // not on a port, or the core offers nothing to choose there
    }
    const LibretroPortControllers* types = &host->input_info.ports[port];
    bool offered = false;
    for (uint32_t t = 0; t < types->count && !offered; t++)
    {
        offered = types->types[t].id == device_id;
    }
    if (!offered)
    {
        return;
    }
    libretro_host_set_port_device(app->session.host, static_cast<unsigned>(port), device_id);
    settings_set_int(&app->settings, SettingLayer::core, PICCOLO_INPUT_PORT_DEVICES[port],
                     static_cast<int32_t>(device_id));
    if (!app->settings_window.open)
    {
        save_settings(app);
    }
    SDL_Log("Network: %s set port %d to device %u", device, port + 1, device_id);
}

// Who plays on piccolo's ports now (devices): only controllers that joined
// a port and whose device is connected. `peer` is the device asking (its own
// pads are marked).
static void net_devices_for(const App* app, uint32_t peer, std::vector<NetDevice>* devices)
{
    devices->clear();
    for (size_t c = 0; c < app->input.controllers.size(); c++)
    {
        uint32_t controller = static_cast<uint32_t>(c);
        uint8_t ports = 0;
        for (uint32_t p = 0; p < INPUT_PORT_COUNT; p++)
        {
            if (input_port_has(&app->input, p, controller))
            {
                ports = static_cast<uint8_t>(ports | (1u << p));
            }
        }
        const InputDevice* device = input_devices_find_const(&app->input.devices, app->input.controllers[c].device_id);
        if (ports == 0 || device == nullptr || !device->connected)
        {
            continue;
        }
        NetDevice entry;
        entry.controller = controller;
        entry.device = device->id;
        entry.ports = ports;
        entry.name = input_controller_label(&app->input, controller);
        entry.kind = device->kind == InputDeviceKind::keyboard ? NetDeviceKind::keyboard
                     : device->kind == InputDeviceKind::mouse  ? NetDeviceKind::mouse
                                                                : NetDeviceKind::gamepad;
        // A paired device's pad: whose, and which of its pads.
        for (uint32_t owner = 0; owner < NET_MAX_PEERS; owner++)
        {
            for (uint32_t pad = 0; pad < NET_INPUT_MAX_PADS; pad++)
            {
                const InputNetPad* net_pad = &app->input_net.pads[owner][pad];
                if (net_pad->active && net_pad->device == device->id)
                {
                    entry.kind = NetDeviceKind::network;
                    entry.owner = owner == peer ? NetDeviceOwner::yours : NetDeviceOwner::other;
                    entry.pad = static_cast<uint8_t>(pad);
                }
            }
        }
        devices->push_back(entry);
    }
}

// A summary of net_devices_for, compared every loop (no allocation).
static uint64_t net_devices_summary(const App* app, uint32_t peer)
{
    uint64_t summary = 1469598103934665603ull;
    for (size_t c = 0; c < app->input.controllers.size(); c++)
    {
        uint32_t controller = static_cast<uint32_t>(c);
        uint32_t ports = 0;
        for (uint32_t p = 0; p < INPUT_PORT_COUNT; p++)
        {
            ports |= input_port_has(&app->input, p, controller) ? (1u << p) : 0u;
        }
        const InputDevice* device = input_devices_find_const(&app->input.devices, app->input.controllers[c].device_id);
        if (ports == 0 || device == nullptr || !device->connected)
        {
            continue;
        }
        uint32_t values[3] = {controller, device->id, ports};
        for (uint32_t i = 0; i < 3; i++)
        {
            summary = (summary ^ values[i]) * 1099511628211ull;
        }
    }
    return (summary ^ peer) * 1099511628211ull;
}

static void net_send_devices(App* app, uint32_t peer, uint32_t id)
{
    std::vector<NetDevice> devices;
    net_devices_for(app, peer, &devices);
    size_t size = net_settings_write_devices(app->net_remote.reply.data(), app->net_remote.reply.size(), id, &devices);
    net_access_send(&app->net_service.access, peer, NetChannel::control, app->net_remote.reply.data(), size);
    app->net_devices_sent[peer] = net_devices_summary(app, peer);
}

// device_port: that controller on that port only (-1: on none), as the quick
// menu's CONTROLS does it. Nothing is saved (START joins again next time).
// A stale controller / device pair (the list changed meanwhile) is ignored:
// the summary check sends the device a fresh `devices`.
static void net_move_device(App* app, uint32_t controller, uint32_t device_id, int8_t port, const char* device)
{
    if (controller >= app->input.controllers.size() || app->input.controllers[controller].device_id != device_id ||
        port >= static_cast<int8_t>(INPUT_PORT_COUNT) || port < -1)
    {
        return;
    }
    for (uint32_t p = 0; p < INPUT_PORT_COUNT; p++)
    {
        input_port_remove(&app->input, p, controller);
    }
    if (port >= 0)
    {
        input_port_add(&app->input, static_cast<uint32_t>(port), controller);
    }
    SDL_Log("Network: %s moved %s to port %d", device, input_controller_label(&app->input, controller).c_str(), port + 1);
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
            if (app->net_service.access.peers[i].state == NetAccessPeerState::authorized &&
                net_peer_access(app, i) == NetAccessLevel::full)
            {
                net_access_send(&app->net_service.access, i, NetChannel::control, app->net_remote.reply.data(), size);
            }
        }
        size = net_remote_next_change(&app->net_remote, &app->settings);
    }
    // A PAUSE from a device ends with another game (or none).
    if (app->net_paused && game_running(app) != app->net_actions_sent.running)
    {
        app->net_paused = false;
    }
    // A device whose access changed (NETWORK): its new welcome went out with
    // net_access_set_level; its action list follows at once, and what its new
    // level doesn't cover stops: remote play, a FAST FORWARD it holds, a PAUSE
    // it made (STREAM since 2026-11-20). Speed tests and pads are GAMEPAD.
    for (uint32_t i = 0; i < NET_MAX_PEERS; i++)
    {
        if (app->net_service.access.peers[i].state != NetAccessPeerState::authorized ||
            net_peer_access(app, i) == app->net_actions_level[i])
        {
            continue;
        }
        net_send_actions(app, i, 0);
        NetAccessLevel level = net_peer_access(app, i);
        if (!net_access_allows(level, NetAccessLevel::stream))
        {
            app->net_fast_forward[i] = false;
            if (app->headless.on && app->headless.peer == i)
            {
                headless_back(app, true);   // remote play (and so the game) needs STREAM
                headless_tell(app);
            }
            if (app->net_paused && app->net_paused_by == i)
            {
                app->net_paused = false;   // it can't resume it any more
                set_status(app, piccolo_text(&app->strings, Str::status_resumed));
            }
            net_service_set_stream(&app->net_service, i, 0);   // remote play needs STREAM
            app->stream_tuning_set[i] = false;
        }
    }
    for (uint32_t i = 0; i < NET_MAX_PEERS; i++)
    {
        if (app->net_devices_wanted[i] && app->net_service.access.peers[i].state == NetAccessPeerState::authorized &&
            net_peer_access(app, i) == NetAccessLevel::full && net_devices_summary(app, i) != app->net_devices_sent[i])
        {
            net_send_devices(app, i, 0);   // someone joined, left or moved
        }
    }
    for (uint32_t i = 0; i < NET_MAX_PEERS; i++)
    {
        if (app->net_service.access.peers[i].state == NetAccessPeerState::authorized &&
            net_controllers_summary(app, i) != app->net_controllers_sent[i])
        {
            net_send_controllers(app, i, 0);   // a pad joined or left a port, another game, another device
        }
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
        net_access_send(&app->net_service.access, event->peer, NetChannel::control, app->net_remote.reply.data(), size);
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
        net_access_send(&app->net_service.access, event->peer, NetChannel::control, app->net_remote.reply.data(), size);
        return;
    }
    std::string key;
    bool pressed = false;
    if (net_settings_read_action(event->data, event->size, &key, &pressed))
    {
        net_do_action(app, event->peer, &key, pressed, device);
        return;
    }
    if (event->peer >= NET_MAX_PEERS)
    {
        return;
    }
    if (net_settings_read_controllers_request(event->data, event->size, &id))
    {
        net_send_controllers(app, event->peer, id);
        return;
    }
    uint8_t pad = 0;
    uint32_t device_id = 0;
    if (net_settings_read_port_device(event->data, event->size, &pad, &device_id))
    {
        net_change_port_device(app, event->peer, pad, device_id, device);
        return;
    }
    // devices / device_port: FULL (net_access let them through only then).
    if (net_settings_read_devices_request(event->data, event->size, &id))
    {
        app->net_devices_wanted[event->peer] = true;
        net_send_devices(app, event->peer, id);
        return;
    }
    uint32_t controller = 0;
    int8_t port = 0;
    if (net_settings_read_device_port(event->data, event->size, &controller, &device_id, &port))
    {
        net_move_device(app, controller, device_id, port, device);
    }
}

// A speed test request (net_speed.h; net_access let it through only for
// STREAM devices). One at a time: a request during another is ignored.
static void net_speed_request(App* app, const NetAccessEvent* event)
{
    uint32_t id = 0;
    NetSpeedDirection direction = NetSpeedDirection::down;
    uint32_t rate = 0;
    uint32_t duration = 0;
    if (!net_speed_read_request(event->data, event->size, &id, &direction, &rate, &duration) ||
        app->net_speed_sender.active || app->net_speed_reporting || app->net_speed_receiver.active)
    {
        return;
    }
    double now = net_seconds();
    if (direction == NetSpeedDirection::down)
    {
        net_speed_sender_start(&app->net_speed_sender, id, event->peer, rate, duration, now);
    }
    else
    {
        net_speed_receiver_start(&app->net_speed_receiver, id, event->peer,
                                 now + static_cast<double>(duration) / 1000.0 + NET_SPEED_GRACE_SECONDS);
    }
}

// speed_result: one step of a device's speed test, as it measured it.
static void net_speed_result(App* app, const NetAccessEvent* event)
{
    NetSpeedResult result;
    if (event->device < 0 || static_cast<size_t>(event->device) >= app->net_service.access.trust.devices.size() ||
        !net_speed_read_result(event->data, event->size, &result))
    {
        return;
    }
    const uint8_t* key = app->net_service.access.trust.devices[static_cast<size_t>(event->device)].key;
    NetSpeedSummary* summary = nullptr;
    for (size_t i = 0; i < app->net_speed_results.size() && summary == nullptr; i++)
    {
        if (std::memcmp(app->net_speed_results[i].key, key, NET_KEY_SIZE) == 0)
        {
            summary = &app->net_speed_results[i];
        }
    }
    if (summary == nullptr)
    {
        NetSpeedSummary entry;
        std::memcpy(entry.key, key, NET_KEY_SIZE);
        app->net_speed_results.push_back(entry);
        summary = &app->net_speed_results.back();
    }
    bool down = result.direction == NetSpeedDirection::down;
    uint32_t* best = down ? &summary->down_kbps : &summary->up_kbps;
    uint32_t* last_asked = down ? &summary->down_last_asked : &summary->up_last_asked;
    bool* tested = down ? &summary->down_tested : &summary->up_tested;
    if (result.asked_kbps <= *last_asked)
    {
        *best = 0;   // a new test (its first, lowest step)
    }
    *last_asked = result.asked_kbps;
    *tested = true;
    if (result.loss_per_mille <= NET_WINDOW_SPEED_MAX_LOSS && result.got_kbps > *best)
    {
        *best = result.got_kbps;
    }
}

// Every loop: the down test's packets that are due, and the reports.
static void net_speed_update(App* app)
{
    double now = net_seconds();
    NetSpeedSender* sender = &app->net_speed_sender;
    if (sender->active)
    {
        uint8_t packet[NET_SPEED_PACKET_SIZE];
        uint32_t due = net_speed_sender_due(sender, now);
        for (uint32_t i = 0; i < due; i++)
        {
            size_t size = net_speed_write_data(packet, sizeof(packet), sender->id, sender->sent);
            net_access_send(&app->net_service.access, sender->peer, NetChannel::input, packet, size);
            net_speed_sender_sent(sender, size);   // counted as sent even if ENet dropped it here: that's loss too
        }
        app->net_speed_reporting = !sender->active;
    }
    uint8_t out[64];
    if (app->net_speed_reporting)
    {
        NetSpeedReport report;
        report.id = sender->id;
        report.direction = NetSpeedDirection::down;
        report.packets = sender->sent;
        report.bytes = sender->sent_bytes;
        report.seconds = sender->duration;
        size_t size = net_speed_write_report(out, sizeof(out), &report);
        net_access_send(&app->net_service.access, sender->peer, NetChannel::control, out, size);
        app->net_speed_reporting = false;
    }
    NetSpeedReceiver* receiver = &app->net_speed_receiver;
    if (receiver->active && now >= receiver->until)
    {
        NetSpeedReport report;
        net_speed_receiver_report(receiver, NetSpeedDirection::up, &report);
        size_t size = net_speed_write_report(out, sizeof(out), &report);
        net_access_send(&app->net_service.access, receiver->peer, NetChannel::control, out, size);
        receiver->active = false;
    }
}

// Which streaming device's stream_tuning applies (the newest request of the
// ones still streaming), and what it gives: told to the devices that asked,
// again whenever it changes (AUTO coming down).
static void net_follow_stream_tuning(App* app)
{
    int32_t from = -1;
    for (uint32_t peer = 0; peer < NET_MAX_PEERS; peer++)
    {
        if (app->stream_tuning_set[peer] && app->net_service.stream.flags[peer] != 0 &&
            (from < 0 || app->stream_tuning_order[peer] > app->stream_tuning_order[from]))
        {
            from = static_cast<int32_t>(peer);
        }
    }
    bool changed = from != app->stream_tuning_from ||
                   (from >= 0 && (app->stream_tuning_asked[from].delay != app->stream_tuning.delay ||
                                  app->stream_tuning_asked[from].delay_ms != app->stream_tuning.delay_ms));
    if (changed)
    {
        app->stream_auto_delay = AutoFrameDelay{};   // a new request searches from the top
        app->stream_delay_last_ns = 0;
    }
    app->stream_tuning_from = from;
    app->stream_tuning_on = from >= 0;
    if (from >= 0)
    {
        app->stream_tuning = app->stream_tuning_asked[from];
    }
    // What applies, as each asking device should hear it.
    NetStreamTuning result;
    result.delay = app->stream_tuning_on ? app->stream_tuning.delay : NetStreamDelay::host;
    result.delay_ms = static_cast<uint8_t>(app->frame_delay_ms < 255 ? app->frame_delay_ms : 255);
    // What the running core really does (only cores with their own, for now).
    result.run_ahead = app->session.host != nullptr && game_running(app)
                           ? static_cast<uint8_t>(libretro_host_run_ahead_applied(app->session.host))
                           : 0;
    result.flags = app->shell.renderer.wait_for_present ? NET_STREAM_TUNING_WAITS_FOR_PRESENT : 0;
    for (uint32_t peer = 0; peer < NET_MAX_PEERS; peer++)
    {
        if (!app->stream_tuning_set[peer])
        {
            continue;
        }
        const NetStreamTuning* sent = &app->stream_tuning_sent[peer];
        if (app->stream_tuning_told[peer] && sent->delay == result.delay && sent->delay_ms == result.delay_ms &&
            sent->run_ahead == result.run_ahead && sent->flags == result.flags)
        {
            continue;
        }
        uint8_t out[8];
        size_t size = net_stream_write_tuning(out, sizeof(out), NET_STREAM_TUNING_RESULT, &result);
        net_access_send(&app->net_service.access, peer, NetChannel::control, out, size);
        app->stream_tuning_sent[peer] = result;
        app->stream_tuning_told[peer] = true;
    }
}

// stream_start / stream_stop / keyframe_request (net_stream.h; net_access let
// them through only for STREAM devices).
static void net_stream_message(App* app, const NetAccessEvent* event, const char* device)
{
    if (event->size == 0)
    {
        return;
    }
    uint8_t type = event->data[0];
    uint8_t flags = 0;
    bool headless_on = false;
    if (net_stream_read_start(event->data, event->size, &flags))
    {
        net_service_set_stream(&app->net_service, event->peer, flags);
        SDL_Log("Network: %s started remote play%s%s", device, (flags & NET_STREAM_WANT_VIDEO) != 0 ? " (video)" : "",
                (flags & NET_STREAM_WANT_AUDIO) != 0 ? " (audio)" : "");
    }
    else if (type == NET_STREAM_STOP && event->size == 1)
    {
        if (app->headless.on && app->headless.peer == event->peer)
        {
            headless_back(app, true);   // it no longer sees the game
            headless_tell(app);
        }
        net_service_set_stream(&app->net_service, event->peer, 0);
        if (event->peer < NET_MAX_PEERS)
        {
            app->stream_tuning_set[event->peer] = false;
        }
        SDL_Log("Network: %s stopped remote play", device);
    }
    else if (type == NET_STREAM_KEYFRAME_REQUEST && event->size == 1)
    {
        net_service_request_keyframe(&app->net_service, event->peer);
    }
    else if (net_stream_read_go_headless(event->data, event->size, &headless_on))
    {
        // Taken only by a device streaming a running game, and only if no
        // other device holds it; given back only by the one holding it.
        bool streaming = event->peer < NET_MAX_PEERS && app->net_service.stream.flags[event->peer] != 0;
        if (headless_on && !app->headless.on && game_running(app) && streaming)
        {
            headless_enter(&app->headless, app->shell.window, event->peer, &app_icon_images[2],
                           piccolo_text(&app->strings, Str::headless_tooltip),
                           piccolo_text(&app->strings, Str::headless_show),
                           piccolo_text(&app->strings, Str::headless_play_here));
            SDL_Log("Headless: %s took the game", device);
        }
        else if (!headless_on && app->headless.on && app->headless.peer == event->peer)
        {
            headless_back(app, false);
        }
        headless_tell(app);   // the asker learns the result either way
    }
    else if (event->peer < NET_MAX_PEERS &&
             net_stream_read_tuning(event->data, event->size, NET_STREAM_TUNING, &app->stream_tuning_asked[event->peer]))
    {
        app->stream_tuning_set[event->peer] = true;
        app->stream_tuning_order[event->peer] = ++app->stream_tuning_count;
        app->stream_tuning_told[event->peer] = false;   // answered with what applies, below
        SDL_Log("Network: %s asks for frame delay mode %u (%u ms), run-ahead %u", device,
                static_cast<unsigned>(app->stream_tuning_asked[event->peer].delay),
                static_cast<unsigned>(app->stream_tuning_asked[event->peer].delay_ms),
                static_cast<unsigned>(app->stream_tuning_asked[event->peer].run_ahead));
    }
}

// After the frames of this loop ran: the frame shown and the audio heard go
// to the network thread (it codes and sends; this loop never waits for it).
// With run-ahead (later) this stays the shown frame and the real timeline's
// audio, numbered by this loop.
static void net_stream_frame(App* app)
{
    if (!app->net_streaming || !game_running(app))
    {
        app->session.captured_audio.clear();
        return;
    }
    app->stream_loop++;
    const LibretroHost* host = app->session.host;
    if (!app->session.captured_audio.empty())
    {
        net_service_submit_audio(&app->net_service, app->session.captured_audio.data(),
                                 app->session.captured_audio.size() / 2, app->stream_loop);
        app->session.captured_audio.clear();
    }
    const LibretroVideoFrame* video = &host->video;
    FramePixelFormat format = FramePixelFormat::xrgb8888;
    if (video->format == RETRO_PIXEL_FORMAT_RGB565)
    {
        format = FramePixelFormat::rgb565;
    }
    else if (video->format == RETRO_PIXEL_FORMAT_0RGB1555)
    {
        format = FramePixelFormat::xrgb1555;
    }
    else if (video->format != RETRO_PIXEL_FORMAT_XRGB8888)
    {
        return;   // the HDR formats aren't streamed
    }
    if (video->pixels == nullptr || video->width == 0 || video->height == 0 || video->serial == app->stream_serial)
    {
        return;   // nothing new to show
    }
    app->stream_serial = video->serial;
    NetStreamInfo info;
    info.width = video->width;
    info.height = video->height;
    info.format = format;
    info.aspect = static_cast<double>(host->av_info.geometry.aspect_ratio);
    info.fps = host->av_info.timing.fps;
    info.sample_rate = host->av_info.timing.sample_rate;
    net_service_submit_frame(&app->net_service, video->pixels, video->width, video->height, format, app->stream_loop,
                             &info);
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
    // Called from the main loop, outside net_update: the lock is ours to take.
    net_service_lock(&app->net_service);
    net_access_send(&app->net_service.access, app->net_load_peer, NetChannel::control, reply, size);
    net_service_unlock(&app->net_service);
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
        net_access_send(&app->net_service.access, event->peer, NetChannel::control, app->net_games.reply.data(), size);
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
        net_access_send(&app->net_service.access, event->peer, NetChannel::control, reply, reply_size);
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
// The network library and this device's key (made once, kept in the
// settings folder): for the listener and for SHARE's connection alike.
static bool net_prepare(App* app)
{
    if (app->paths.settings.empty())
    {
        return false;   // nowhere to keep the device key: networking stays off
    }
    if (!app->net_ready)
    {
        app->net_ready = net_init();
        if (!app->net_ready)
        {
            return false;
        }
    }
    if (!app->net_has_identity)
    {
        std::string identity_path = app->paths.settings + "/net_identity.cfg";
        app->net_has_identity = net_identity_load_or_create(identity_path.c_str(), &app->net_identity);
        if (!app->net_has_identity)
        {
            return false;
        }
        net_key_fingerprint(app->net_identity.public_key, app->net_fingerprint, sizeof(app->net_fingerprint));
        SDL_Log("Network: this device is %s", app->net_fingerprint);
    }
    return true;
}

static void net_start(App* app)
{
    // Any failure below is remembered with the port it was tried on, so it
    // isn't retried every frame (net_update).
    app->net_port = static_cast<uint16_t>(settings_get_int(&app->settings, PICCOLO_NET_PORT));
    app->net_listen_failed = true;
    if (!net_prepare(app))
    {
        return;
    }
    app->net_service.access = NetAccess{};
    net_games_init(&app->net_games);
    if (!app->net_remote_ready)
    {
        app->net_remote_ready = net_remote_init(&app->net_remote, &app->settings);
    }
    std::string devices_path = app->paths.settings + "/net_devices.cfg";
    if (!net_service_start(&app->net_service, app->net_port, settings_get_text(&app->settings, PICCOLO_NET_NAME),
                           &app->net_identity, devices_path.c_str()))
    {
        return;
    }
    app->net_listen_failed = false;
    app->net_running = true;
    app->net_beacon_applied.clear();   // a new service: net_update hands it the beacon again
    app->net_share_set = false;        // and its shared folders
}

static void net_stop(App* app)
{
    if (app->net_running)
    {
        for (uint32_t peer = 0; peer < NET_MAX_PEERS; peer++)
        {
            input_net_peer_gone(&app->input_net, &app->input, peer);   // their pads are unplugged
        }
        net_service_stop(&app->net_service);
        app->net_running = false;
    }
}

static void net_shutdown_all(App* app)
{
    net_share_link_stop(&app->share_link);
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
static void net_update_locked(App* app);

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
        headless_back(app, false);   // networking went off: nobody streams
        return;
    }
    // INTERNET and BEACON SERVER: the service resolves the name on its own
    // thread. Only when they change (an empty name is off too).
    std::string beacon = settings_get_bool(&app->settings, PICCOLO_NET_INTERNET)
                             ? settings_get_text(&app->settings, PICCOLO_NET_BEACON)
                             : "";
    if (beacon != app->net_beacon_applied)
    {
        app->net_beacon_applied = beacon;
        net_service_set_beacon(&app->net_service, beacon.c_str());
    }
    // The shared folders OWNER devices reach (piccolo-share): looked at
    // again only when a setting changed, handed over only when they did.
    bool share_check = !app->net_share_set || app->settings.revision != app->net_share_revision;
    std::vector<std::string> share_games;
    std::string share_saves;
    std::string share_states;
    std::string share_key;
    if (share_check)
    {
        app->net_share_revision = app->settings.revision;
        share_folder_paths(app, &share_games, &share_saves, &share_states);
        share_key = share_saves + "\n" + share_states;
        for (size_t i = 0; i < share_games.size(); i++)
        {
            share_key += "\n" + share_games[i];
        }
    }
    // The network thread polls; this loop handles what it queued, holding
    // the lock for all of it (the NetAccess, its peers and lists).
    net_service_lock(&app->net_service);
    if (share_check && (!app->net_share_set || share_key != app->net_share_applied))
    {
        app->net_share_applied = share_key;
        app->net_share_set = true;
        net_service_set_share_folders(&app->net_service, &share_games, share_saves.c_str(), share_states.c_str());
    }
    net_update_locked(app);
    net_service_unlock(&app->net_service);
}

static void net_update_locked(App* app)
{
    // GO HEADLESS ends from the tray, or when no game runs any more.
    if (app->headless.on && (app->headless.back_asked || !game_running(app)))
    {
        headless_back(app, false);
        headless_tell(app);
    }
    // A new NAME is used at once (discovery and pairing show it).
    const char* name = settings_get_text(&app->settings, PICCOLO_NET_NAME);
    if (app->net_service.access.name != name)
    {
        app->net_service.access.name = name;
        app->net_service.access.session.host.name = name;
    }

    NetAccessEvent event;
    while (net_service_next(&app->net_service, &event))
    {
        char address[64];
        net_address_to_text(event.address, address, sizeof(address));
        const char* device = event.device >= 0 && static_cast<size_t>(event.device) < app->net_service.access.trust.devices.size()
                                 ? app->net_service.access.trust.devices[static_cast<size_t>(event.device)].name.c_str()
                                 : "?";
        switch (event.type)
        {
        case NetAccessEventType::authorized:
            SDL_Log("Network: \"%s\" connected from %s", device, address);
            net_send_game_state(app, event.peer);   // what runs, from the start
            net_send_actions(app, event.peer, 0);
            net_send_controllers(app, event.peer, 0);
            break;
        case NetAccessEventType::paired:
        {
            char status[256];
            std::snprintf(status, sizeof(status), "%s %s", piccolo_text(&app->strings, Str::status_net_paired), device);
            set_status(app, status);
            net_send_game_state(app, event.peer);
            net_send_actions(app, event.peer, 0);
            net_send_controllers(app, event.peer, 0);
            break;
        }
        case NetAccessEventType::closed:
            if (app->headless.on && app->headless.peer == event.peer)
            {
                headless_back(app, true);   // the device that had the game left
                headless_tell(app);
            }
            if (event.peer < NET_MAX_PEERS && app->net_speed_sender.peer == event.peer)
            {
                app->net_speed_sender.active = false;   // its test ends with it
                app->net_speed_reporting = false;
            }
            if (event.peer < NET_MAX_PEERS && app->net_speed_receiver.peer == event.peer)
            {
                app->net_speed_receiver.active = false;
            }
            net_service_set_stream(&app->net_service, event.peer, 0);   // its stream ends with it
            if (event.peer < NET_MAX_PEERS)
            {
                app->stream_tuning_set[event.peer] = false;   // and what it asked for
            }
            if (event.peer < NET_MAX_PEERS)
            {
                app->net_fast_forward[event.peer] = false;   // a dropped device doesn't keep it held
                app->net_peer_language[event.peer][0] = '\0';
                app->net_devices_wanted[event.peer] = false;
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
        case NetAccessEventType::unpaired:
        {
            // The device forgot this piccolo, so it's gone from PAIRED DEVICES too.
            char status[256];
            std::snprintf(status, sizeof(status), "%s %s", piccolo_text(&app->strings, Str::status_net_unpaired),
                          event.name);
            set_status(app, status);
            SDL_Log("Network: \"%s\" unpaired itself", event.name);
            break;
        }
        case NetAccessEventType::message:
            if (event.channel == NetChannel::input && event.size > 0 && event.data[0] == NET_INPUT_TYPE_SPEED)
            {
                // A speed test's packet (up), from the device running the test.
                if (event.peer == app->net_speed_receiver.peer)
                {
                    net_speed_receive(&app->net_speed_receiver, event.data, event.size, net_seconds());
                }
            }
            else if (event.channel == NetChannel::input && event.device >= 0)
            {
                const NetTrustedDevice* paired = &app->net_service.access.trust.devices[static_cast<size_t>(event.device)];
                input_net_set_hotkeys(&app->input_net, &app->input, event.peer,
                                      net_access_allows(event.access, NetAccessLevel::stream));
                input_net_receive(&app->input_net, &app->input, event.peer, paired->key, paired->name.c_str(),
                                  event.data, event.size);
            }
            else if (event.channel == NetChannel::control)
            {
                if (event.size > 0 && event.data[0] == NET_SPEED_REQUEST)
                {
                    net_speed_request(app, &event);
                }
                if (event.size > 0 && event.data[0] == NET_SPEED_RESULT)
                {
                    net_speed_result(app, &event);
                }
                net_stream_message(app, &event, device);
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
    net_speed_update(app);
    // Remote play: frames and audio are handed over only while someone streams.
    app->net_streaming = net_service_streaming(&app->net_service);
    if (app->net_streaming && !app->session.capture_audio)
    {
        app->session.captured_audio.reserve(16384);   // once: a few frames' worth
    }
    app->session.capture_audio = app->net_streaming;
    net_follow_stream_tuning(app);
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
    config.identifier = "site.multitap.piccolo";
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
    // A build from a branch other than main has the branch in its version ("0.3-osk"):
    // show it in the window title, so a test build says which branch opened it.
    constexpr std::string_view version = PICCOLO_VERSION;
    if (version.find('-') != std::string_view::npos)
    {
        SDL_SetWindowTitle(app->shell.window, "piccolo " PICCOLO_VERSION);
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
    if (app_paths_init(&app->paths, bundled_files_folder_name(), "piccolo.cfg"))
    {
        bundled_files_install_cores(app->paths.cores.c_str());   // before the catalog first looks
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

    // The files that come with piccolo (layouts, themes, shaders): next to the
    // program; on Android a copy out of the APK (bundled_files.h).
    std::string program_folder = bundled_files_data_folder();

    // librashader (shader presets): next to the program, in piccolo's folder,
    // or wherever the system finds it (on Android: the APK's native
    // libraries, where dlopen looks for a bare name). Without it, presets are off.
    std::vector<std::string> shader_library_folders;
    const char* program_folder_for_shaders = SDL_GetBasePath();   // SDL owns it
    if (program_folder_for_shaders != nullptr)
    {
        shader_library_folders.push_back(program_folder_for_shaders);
    }
    shader_library_folders.push_back(app->paths.root);
    shader_library_load(&app->shader_library, shader_library_folders);
    // piccolo's own presets: the user's copy in piccolo's folder wins over
    // the ones that come with piccolo (next to the program), like lang/.
    {
        std::string user_folder = app->paths.root.empty() ? std::string() : app->paths.root + "/shaders/";
        std::string bundled_folder = program_folder + "shaders/";
        host_fs_forward_slashes(bundled_folder.data());
        const char* names[4] = {"pass0.slangp", "scale-nearest.slangp", "scale-bilinear.slangp",
                                "scale-sharp-bilinear.slangp"};
        std::string* targets[4] = {&app->shader_pass0_path, &app->shader_scale_paths[0], &app->shader_scale_paths[1],
                                   &app->shader_scale_paths[2]};
        for (uint32_t i = 0; i < 4; i++)
        {
            std::string user_copy = user_folder.empty() ? std::string() : user_folder + names[i];
            *targets[i] = !user_copy.empty() && host_fs_exists(user_copy.c_str()) ? user_copy : bundled_folder + names[i];
        }
        // The curated presets: the copy in piccolo's folder when the build
        // put one there, else the one next to the program.
        std::string user_presets = user_folder.empty() ? std::string() : user_folder + "presets";
        app->shader_presets_folder = !user_presets.empty() && host_fs_exists(user_presets.c_str()) ? user_presets
                                                                                                  : bundled_folder + "presets";
    }

    // Controllers: MAP CONTROLLER's mappings are in their profiles (input.cfg);
    // settings/gamecontrollerdb.txt, if the user drops one in (SDL's format),
    // adds to the ones SDL knows.
    std::string database_path = app->paths.settings.empty() ? "" : app->paths.settings + "/gamecontrollerdb.txt";
    // Controller layouts: the ones next to the program, then piccolo's folder's
    // (the same id there wins), and GENERIC built in if neither has it.
    std::string bundled_layouts = program_folder + "layouts";
    host_fs_forward_slashes(bundled_layouts.data());
    input_layouts_load_directory(&app->layouts, bundled_layouts.c_str());
    if (!app->paths.root.empty())
    {
        input_layouts_load_directory(&app->layouts, (app->paths.root + "/layouts").c_str());
    }
    input_layouts_ensure_generic(&app->layouts);
    // Interface themes from files: the ones next to the program, then piccolo's
    // folder's (the same name there wins). The built-in ones are in retro_ui.cpp.
    std::string bundled_themes = program_folder + "themes";
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
        if (app->headless.on)
        {
            // No presents (a hidden window gets no frame callbacks): the
            // Exact timer paces the core, frames only go to the stream.
            app->session.timing.mode = TimingMode::exact;
        }
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
        // RUN-AHEAD: the setting, or a streaming device's tuning (in memory
        // only) when it isn't "host's".
        {
            int32_t run_ahead = settings_get_int(&app->settings, PICCOLO_LATENCY_RUN_AHEAD);
            if (stream_delay_overrides(app))
            {
                run_ahead = app->stream_tuning.run_ahead;
            }
            if (app->session.host != nullptr)
            {
                libretro_host_set_run_ahead(app->session.host, static_cast<unsigned>(run_ahead > 0 ? run_ahead : 0));
            }
        }
        // A streaming device's frame delay needs present waits (piccolo's
        // frame delay works only that way): turned on here, in memory only.
        app->shell.renderer.wait_for_present = settings_get_bool(&app->settings, PICCOLO_LATENCY_WAIT_FOR_PRESENT) ||
                                               (stream_delay_overrides(app) &&
                                                app->stream_tuning.delay != NetStreamDelay::off);
        piccolo_settings_audio(&app->settings, &app->session.audio_config);
        if (app->headless.on)
        {
            app->session.audio_config.muted = true;   // the device plays it
        }
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
        if (ran == CoreSessionFrame::ran)
        {
            net_stream_frame(app);
            measure_shader_check(app);
        }
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

        if (!app->headless.on && app_shell_begin_frame(&app->shell))
        {
            app->ui_scale = retro_ui_scale_for_window(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);

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
            quick.core_name = app->core_name;
            quick.game_name = app->game_name;
            quick.shaders_available = app->shader_library.loaded;
            QuickMenuAction quick_action = quick_menu_draw(&app->quick_menu, &quick, app->pixel_font, app->ui_scale);
            draw_remap_prompt(app);
            if (app->quick_menu.choose_shader_request)
            {
                app->quick_menu.choose_shader_request = false;
                open_shader_picker(app, app->quick_menu.shader_scope);
            }
            draw_shader_picker(app);   // over the QUICK MENU
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
            // The core downloader: pick up what finished; the catalog sees
            // installed or removed cores right away.
            core_updater_poll(&app->core_updater);
            if (app->core_updater.cores_changed)
            {
                app->core_updater.cores_changed = false;
                core_catalog_scan(&app->catalog, game_running(app) ? app->session.host : nullptr);
            }
            std::string running_file = running_core_file(app);
            CoresWindowContext cores_context = {};
            cores_context.strings = &app->strings;
            cores_context.updater = &app->core_updater;
            cores_context.running_file = running_file.c_str();
            app->cores_window.show_non_free = settings_get_bool(&app->settings, PICCOLO_CORES_SHOW_NON_FREE);
            app->cores_window.show_legacy = settings_get_bool(&app->settings, PICCOLO_CORES_SHOW_LEGACY);
            app->cores_window.show_experimental = settings_get_bool(&app->settings, PICCOLO_CORES_SHOW_EXPERIMENTAL);
            cores_window_draw(&app->cores_window, &cores_context, app->pixel_font, app->ui_scale);

            // SHARE: this side from our folders, the other from the connection.
            if (app->share_window.open)
            {
                ShareTransferLine share_line;
                share_update(app, &share_line);
                ShareWindowContext share_context;
                share_context.strings = &app->strings;
                share_context.local_folders = &app->share_folders;
                share_context.link = net_share_link_started(&app->share_link) ? &app->share_status : nullptr;
                share_context.line = &share_line;
                share_window_draw(&app->share_window, &share_context, app->pixel_font, app->ui_scale);
            }
            if (app->cores_window.show_changed)
            {
                app->cores_window.show_changed = false;
                settings_set_bool(&app->settings, SettingLayer::global, PICCOLO_CORES_SHOW_NON_FREE,
                                  app->cores_window.show_non_free);
                settings_set_bool(&app->settings, SettingLayer::global, PICCOLO_CORES_SHOW_LEGACY,
                                  app->cores_window.show_legacy);
                settings_set_bool(&app->settings, SettingLayer::global, PICCOLO_CORES_SHOW_EXPERIMENTAL,
                                  app->cores_window.show_experimental);
                save_settings(app);
            }
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
            if (shader_check_asking(app))
            {
                std::string name = app->shader_preset_seen;
                size_t slash = name.find_last_of("/\\");
                name = slash == std::string::npos ? name : name.substr(slash + 1);
                size_t dot = name.rfind('.');
                name = dot == std::string::npos ? name : name.substr(0, dot);
                for (size_t i = 0; i < name.size(); i++)
                {
                    name[i] = static_cast<char>(SDL_toupper(static_cast<unsigned char>(name[i])));   // the pixel font has capitals
                }
                const FrameTiming* timing = &app->session.timing;
                double target = timing->effective == TimingMode::vsync ? timing->display_hz : timing->core_fps;
                ShaderCheckChoice answer = shader_check_window_draw(
                    app->pixel_font, app->ui_scale, &app->strings, name.c_str(), 1e9 / app->shader_check.measured_ns,
                    target, app->shader_check_asked_ms, SDL_GetTicks());
                if (answer != ShaderCheckChoice::none)
                {
                    shader_check_answer(app, answer);
                }
            }
            if (text_input_pending(app))
            {
                TextInputChoice answer = text_input_window_draw(&app->text_input_window, &app->session.host->text_input,
                                                                app->pixel_font, app->ui_scale, &app->strings);
                if (app->session.host->text_input.type == RETRO_EXT_TEXT_INPUT_TYPE_NUMBER)
                {
                    text_input_ui_field_number(&app->shell.text_input);   // the OSK's number keypad
                }
                if (answer == TextInputChoice::ok)
                {
                    libretro_host_text_input_answer(app->session.host, app->text_input_window.buffer);
                }
                else if (answer == TextInputChoice::cancel)
                {
                    libretro_host_text_input_answer(app->session.host, nullptr);
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
                shader_chain_record(&app->shader_chain, &app->shader_library,
                                    renderer_command_buffer(&app->shell.renderer), &app->session.texture);
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

    headless_leave(&app->headless, app->shell.window);   // the tray icon goes, the window comes back for its size
    piccolo_window_capture(&app->window, app->shell.window);
    save_last_used(app);
    save_settings(app);
    save_learned_latency(app);
    save_core_options(app);
    core_updater_shutdown(&app->core_updater);   // cancels a download in progress
    core_session_destroy(&app->session, &app->shell);   // writes save RAM
    remove_running_marker(app);   // quit properly (a marker found at launch stays if no game ran)
    game_mode_set(&app->game_mode, false);
    if (app->audio_ready)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    quick_menu_destroy(&app->quick_menu, &app->shell);
    picture_destroy(&app->picture, &app->shell.vulkan);
    shader_chain_destroy(&app->shader_chain, &app->shader_library, &app->shell.vulkan);
    shader_library_unload(&app->shader_library);
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
