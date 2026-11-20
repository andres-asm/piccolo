#include "net_remote.h"

#include <cstring>

#include "game_mode.h"
#include "net_link.h"
#include "piccolo_settings.h"
#include "piccolo_text.h"

// ---------------------------------------------------------------------------
// Layout: SETTINGS' pages and sections, in its order
// ---------------------------------------------------------------------------

struct LayoutRow
{
    uint32_t    page = SETTING_NO_TEXT;      // string IDs (text_id)
    uint32_t    subpage = SETTING_NO_TEXT;
    uint32_t    section = SETTING_NO_TEXT;
    const char* key = nullptr;
};

static void add_row(std::vector<LayoutRow>* rows, Str page, uint32_t subpage, uint32_t section, const char* key)
{
    LayoutRow row;
    row.page = text_id(page);
    row.subpage = subpage;
    row.section = section;
    row.key = key;
    rows->push_back(row);
}

// Keep in step with settings_window.cpp (what each tab draws).
static void build_layout(std::vector<LayoutRow>* rows)
{
    const uint32_t none = SETTING_NO_TEXT;
    rows->clear();
    add_row(rows, Str::tab_general, none, none, PICCOLO_LANGUAGE);
    add_row(rows, Str::tab_general, none, none, PICCOLO_PAUSE_IN_MENU);
    add_row(rows, Str::tab_general, none, none, PICCOLO_PAUSE_ON_FOCUS_LOSS);
    add_row(rows, Str::tab_general, none, none, PICCOLO_PAUSE_ON_DISCONNECT);
    add_row(rows, Str::tab_general, none, none, PICCOLO_SCREENSAVER_WHEN_PAUSED);
    if (game_mode_supported())
    {
        add_row(rows, Str::tab_general, none, none, PICCOLO_GAME_MODE);
    }

    add_row(rows, Str::tab_interface, none, none, PICCOLO_UI_MENU_BOXES);
    add_row(rows, Str::tab_interface, none, none, PICCOLO_UI_SHADOWS);
    add_row(rows, Str::tab_interface, none, none, PICCOLO_UI_THEME_CURSOR);
    for (uint32_t i = 0; i < piccolo_settings_ui_color_count(); i++)
    {
        add_row(rows, Str::tab_interface, none, text_id(Str::section_colors), piccolo_settings_ui_color_key(i));
    }

    uint32_t picture = text_id(Str::tab_picture);
    add_row(rows, Str::tab_video, picture, none, PICCOLO_VIDEO_ASPECT);
    add_row(rows, Str::tab_video, picture, none, PICCOLO_VIDEO_SCALING);
    add_row(rows, Str::tab_video, picture, none, PICCOLO_VIDEO_MAX_OFFSCREEN);
    add_row(rows, Str::tab_video, picture, none, PICCOLO_VIDEO_FILTER);
    add_row(rows, Str::tab_video, picture, none, PICCOLO_VIDEO_POSITION);
    uint32_t crop = text_id(Str::crop_summary);   // IMAGE SETTINGS' crop
    add_row(rows, Str::tab_video, picture, crop, PICCOLO_VIDEO_CROP_TOP);
    add_row(rows, Str::tab_video, picture, crop, PICCOLO_VIDEO_CROP_BOTTOM);
    add_row(rows, Str::tab_video, picture, crop, PICCOLO_VIDEO_CROP_LEFT);
    add_row(rows, Str::tab_video, picture, crop, PICCOLO_VIDEO_CROP_RIGHT);

    uint32_t display = text_id(Str::tab_display);
    uint32_t window = text_id(Str::section_window);
    add_row(rows, Str::tab_video, display, none, PICCOLO_DISPLAY_FULLSCREEN);
    add_row(rows, Str::tab_video, display, none, PICCOLO_DISPLAY_FULLSCREEN_TYPE);
    add_row(rows, Str::tab_video, display, window, PICCOLO_WINDOW_MODE);
    add_row(rows, Str::tab_video, display, window, PICCOLO_WINDOW_ASPECT);
    add_row(rows, Str::tab_video, display, window, PICCOLO_WINDOW_HEIGHT);
    add_row(rows, Str::tab_video, display, window, PICCOLO_WINDOW_SCALE);
    add_row(rows, Str::tab_video, display, window, PICCOLO_WINDOW_REMEMBER);

    uint32_t sync = text_id(Str::tab_sync);
    uint32_t latency = text_id(Str::section_low_latency);
    add_row(rows, Str::tab_video, sync, none, PICCOLO_VIDEO_TIMING);
    add_row(rows, Str::tab_video, sync, none, PICCOLO_VIDEO_CATCH_UP);
    add_row(rows, Str::tab_video, sync, latency, PICCOLO_LATENCY_FRAMES_IN_FLIGHT);
    add_row(rows, Str::tab_video, sync, latency, PICCOLO_LATENCY_WAIT_FOR_PRESENT);
    add_row(rows, Str::tab_video, sync, latency, PICCOLO_LATENCY_FRAME_DELAY);
    add_row(rows, Str::tab_video, sync, latency, PICCOLO_LATENCY_FRAME_DELAY_AUTO);

    uint32_t advanced = text_id(Str::section_advanced);
    add_row(rows, Str::tab_sound, none, none, PICCOLO_AUDIO_DEVICE);
    add_row(rows, Str::tab_sound, none, none, PICCOLO_AUDIO_VOLUME);
    add_row(rows, Str::tab_sound, none, none, PICCOLO_AUDIO_MUTE);
    add_row(rows, Str::tab_sound, none, none, PICCOLO_AUDIO_LATENCY);
    add_row(rows, Str::tab_sound, none, advanced, PICCOLO_AUDIO_RATE_CONTROL);
    add_row(rows, Str::tab_sound, none, advanced, PICCOLO_AUDIO_MAX_DEVIATION);
    add_row(rows, Str::tab_sound, none, advanced, PICCOLO_AUDIO_QUALITY);
}

static bool offered(const NetRemote* remote, const char* key)
{
    for (size_t i = 0; i < remote->sent.size(); i++)
    {
        if (remote->sent[i].key == key)
        {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

bool net_remote_init(NetRemote* remote, const Settings* settings)
{
    if (!localization_init(&remote->strings, piccolo_strings_table(), text_id(Str::count)))
    {
        return false;
    }
    remote->strings_language = 0;
    remote->reply.resize(NET_LINK_MAX_PAYLOAD);
    std::vector<LayoutRow> rows;
    build_layout(&rows);
    remote->sent.clear();
    for (size_t i = 0; i < rows.size(); i++)
    {
        NetSettingValues values;
        net_settings_values(settings, rows[i].key, &values);
        remote->sent.push_back(values);
    }
    remote->sent_revision = settings->revision;
    remote->sent_core_section = settings->core_section;
    remote->sent_game_section = settings->game_section;
    remote->next_check = 0;
    remote->all_changed = false;
    return true;
}

// The device's language, if piccolo has it; else English.
static void use_language(NetRemote* remote, const char* code, const char* root)
{
    uint32_t language = piccolo_language_from_code(code);
    if (language >= PICCOLO_LANGUAGE_COUNT)
    {
        language = 0;
    }
    if (language != remote->strings_language)
    {
        piccolo_text_load_language(&remote->strings, language, root);
        remote->strings_language = language;
    }
}

static const char* layout_text(const NetRemote* remote, uint32_t id)
{
    return id == SETTING_NO_TEXT ? "" : localization_get(&remote->strings, id);
}

static void describe_all(NetRemote* remote, const Settings* settings, const std::vector<std::string>* audio_devices)
{
    std::vector<LayoutRow> rows;
    build_layout(&rows);
    remote->settings.clear();
    for (size_t i = 0; i < rows.size(); i++)
    {
        NetSetting setting;
        if (!net_settings_describe(settings, &remote->strings, rows[i].key, &setting))
        {
            continue;
        }
        setting.page = layout_text(remote, rows[i].page);
        setting.subpage = layout_text(remote, rows[i].subpage);
        setting.section = layout_text(remote, rows[i].section);
        if (std::strcmp(rows[i].key, PICCOLO_AUDIO_DEVICE) == 0)
        {
            // As SOUND's list: the system default, then what's connected.
            NetSettingChoice system_default;
            system_default.label = localization_get(&remote->strings, text_id(Str::audio_device_default));
            setting.choices.push_back(system_default);
            for (size_t d = 0; d < audio_devices->size(); d++)
            {
                NetSettingChoice device;
                device.key = (*audio_devices)[d];
                setting.choices.push_back(device);
            }
        }
        remote->settings.push_back(setting);
    }
}

size_t net_remote_answer_settings(NetRemote* remote, Settings* settings, const char* root,
                                  const std::vector<std::string>* audio_devices, const uint8_t* data, size_t size)
{
    uint32_t id = 0;
    uint32_t start = 0;
    std::string language;
    if (net_settings_read_request(data, size, &id, &start, &language))
    {
        use_language(remote, language.c_str(), root);
        if (start == 0 || remote->settings.empty())
        {
            describe_all(remote, settings, audio_devices);
        }
        uint8_t layers = 0;
        if (!settings->core_section.empty())
        {
            layers = static_cast<uint8_t>(layers | NET_LAYER_CORE);
        }
        if (!settings->game_section.empty())
        {
            layers = static_cast<uint8_t>(layers | NET_LAYER_GAME);
        }
        // "core:snes9x" / "game:snes9x/Name": the names after the prefix.
        const char* core = settings->core_section.empty() ? "" : settings->core_section.c_str() + 5;
        const char* game = settings->game_section.empty() ? "" : std::strchr(settings->game_section.c_str(), '/');
        game = game == nullptr ? "" : (game[0] == '/' ? game + 1 : game);
        return net_settings_write_result(remote->reply.data(), remote->reply.size(), id, &remote->settings, start,
                                         layers, core, game);
    }

    SettingLayer layer = SettingLayer::global;
    bool reset = false;
    std::string key;
    std::string value;
    if (!net_settings_read_set(data, size, &id, &layer, &reset, &key, &value))
    {
        return 0;
    }
    NetContentStatus status = NetContentStatus::refused;   // NETWORK, folders: not from the network
    if (offered(remote, key.c_str()))
    {
        status = net_settings_apply(settings, layer, reset, key.c_str(), value.c_str());
    }
    NetSettingValues values;
    net_settings_values(settings, key.c_str(), &values);
    return net_settings_write_set_result(remote->reply.data(), remote->reply.size(), id, status, &values);
}

static bool same_values(const NetSettingValues* a, const NetSettingValues* b)
{
    return a->set == b->set && a->global == b->global && a->core == b->core && a->game == b->game &&
           a->effective == b->effective && a->source == b->source;
}

size_t net_remote_next_change(NetRemote* remote, const Settings* settings)
{
    if (settings->revision == remote->sent_revision && remote->next_check == 0)
    {
        return 0;   // nothing changed since the last look
    }
    if (remote->next_check == 0 && (settings->core_section != remote->sent_core_section ||
                                    settings->game_section != remote->sent_game_section))
    {
        // Another core or game: the core / game layers are others. One
        // message says "ask again"; the copy is taken anew.
        remote->sent_core_section = settings->core_section;
        remote->sent_game_section = settings->game_section;
        for (size_t i = 0; i < remote->sent.size(); i++)
        {
            std::string key = remote->sent[i].key;
            net_settings_values(settings, key.c_str(), &remote->sent[i]);
        }
        remote->sent_revision = settings->revision;
        NetSettingValues all;   // key "" = all of them
        return net_settings_write_changed(remote->reply.data(), remote->reply.size(), &all);
    }
    // One change per call, going on from where the last one stopped.
    for (size_t i = remote->next_check; i < remote->sent.size(); i++)
    {
        NetSettingValues now;
        net_settings_values(settings, remote->sent[i].key.c_str(), &now);
        if (!same_values(&now, &remote->sent[i]))
        {
            remote->sent[i] = now;
            remote->next_check = i + 1;
            return net_settings_write_changed(remote->reply.data(), remote->reply.size(), &now);
        }
    }
    remote->next_check = 0;
    remote->sent_revision = settings->revision;
    return 0;
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

struct ActionInfo
{
    const char*    key;
    Str            label;
    NetActionKind  kind;
    NetAccessLevel access;
};

static const ActionInfo ACTIONS[] = {
    {"save_state", Str::menu_save_state, NetActionKind::press, NetAccessLevel::gamepad},
    {"load_state", Str::menu_load_state, NetActionKind::press, NetAccessLevel::gamepad},
    {"slot_previous", Str::action_slot_previous, NetActionKind::press, NetAccessLevel::gamepad},
    {"slot_next", Str::action_slot_next, NetActionKind::press, NetAccessLevel::gamepad},
    {"fast_forward", Str::input_action_hotkey_fast_forward, NetActionKind::hold, NetAccessLevel::gamepad},
    {"pause", Str::action_pause, NetActionKind::toggle, NetAccessLevel::gamepad},
    {"reset", Str::action_reset, NetActionKind::press, NetAccessLevel::full},
    {"close_game", Str::quick_menu_close_game, NetActionKind::press, NetAccessLevel::full},
};
constexpr size_t ACTION_COUNT = sizeof(ACTIONS) / sizeof(ACTIONS[0]);

size_t net_remote_write_actions(NetRemote* remote, uint32_t id, const char* language, const char* root,
                                NetAccessLevel access, const NetRemoteGame* game)
{
    if (language != nullptr)
    {
        use_language(remote, language, root);
    }
    std::vector<NetAction> actions;
    for (size_t i = 0; i < ACTION_COUNT && game->running; i++)
    {
        if (!net_access_allows(access, ACTIONS[i].access))
        {
            continue;
        }
        NetAction action;
        action.key = ACTIONS[i].key;
        action.label = localization_get(&remote->strings, text_id(ACTIONS[i].label));
        action.kind = ACTIONS[i].kind;
        if (action.kind == NetActionKind::toggle)
        {
            action.state = game->paused ? 1 : 0;
        }
        else if (std::strstr(ACTIONS[i].key, "state") != nullptr || std::strstr(ACTIONS[i].key, "slot") != nullptr)
        {
            action.state = game->slot;   // shown next to SAVE / LOAD STATE and the slot buttons
        }
        actions.push_back(action);
    }
    return net_settings_write_actions(remote->reply.data(), remote->reply.size(), id, &actions);
}

bool net_remote_action_allowed(const char* key, NetAccessLevel access, const NetRemoteGame* game)
{
    if (!game->running)
    {
        return false;
    }
    for (size_t i = 0; i < ACTION_COUNT; i++)
    {
        if (std::strcmp(key, ACTIONS[i].key) == 0)
        {
            return net_access_allows(access, ACTIONS[i].access);
        }
    }
    return false;
}
