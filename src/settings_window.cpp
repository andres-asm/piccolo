#include "settings_window.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <SDL3/SDL.h>

#include "game_mode.h"
#include "imgui.h"
#include "libretro_host.h"   // LIBRETRO_PATH_MAX

#include "piccolo_settings.h"
#include "piccolo_text.h"
#include "retro_ui.h"
#include "settings_ui.h"

void settings_window_open(SettingsWindow* window, SettingsTab tab)
{
    if (!window->open)
    {
        window->choose_scope = true;   // needs the settings: done on the next draw
    }
    window->open = true;
    window->tab = tab;
    window->select_tab = true;
}

// A label in the current language with a fixed ImGui ID ("###"), so a
// window or tab keeps its state when the language changes.
static const char* with_id(char* buffer, size_t size, const char* text, const char* id)
{
    std::snprintf(buffer, size, "%s###%s", text, id);
    return buffer;
}

// GLOBAL / THIS CORE / THIS GAME (settings_ui_scope).
static void draw_scope(SettingsWindow* window, const SettingsWindowContext* context)
{
    settings_ui_scope(context->settings, context->strings, context->core_name, context->game_name, &window->scope,
                      &window->choose_scope);
}

// GAME FOLDERS: one row per folder, then "+". Clicking a row's field or
// "+" asks piccolo for CHOOSE FOLDER (choose_game_folder); "-" removes the
// row. Global only, like the other folders (greyed out in other scopes).
// FOLDERS and GAME FOLDERS: one open at a time (opening one closes the
// other; Andrés: their rows needn't line up then). True if `section` is open.
static bool folder_section(SettingsWindow* window, FolderSection section, const char* label, const char* id)
{
    char header[200] = {};
    ImGui::SetNextItemOpen(window->folder_section == section, ImGuiCond_Always);
    bool open = retro_ui_collapsing_header(with_id(header, sizeof(header), label, id), 0);
    if (open)
    {
        window->folder_section = section;
    }
    else if (window->folder_section == section)
    {
        window->folder_section = FolderSection::none;   // closed by its header
    }
    return open;
}

static void draw_game_folders(SettingsWindow* window, const SettingsUiContext* ui)
{
    if (!folder_section(window, FolderSection::game_folders, piccolo_text(ui->strings, Str::section_game_folders),
                        "game_folders"))
    {
        return;
    }
    bool global = ui->scope == SettingLayer::global;
    if (!global)
    {
        ImGui::BeginDisabled();
    }
    uint32_t set = 0;
    while (set < PICCOLO_GAME_FOLDER_MAX && settings_get_text(ui->settings, PICCOLO_PATH_GAMES[set])[0] != '\0')
    {
        set++;
    }
    // The field up to "-" at the right edge.
    float remove_width = ImGui::CalcTextSize("-").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const char* label = piccolo_text(ui->strings, Str::setting_path_games);
    int32_t remove = -1;
    for (uint32_t i = 0; i < set; i++)
    {
        ImGui::PushID(PICCOLO_PATH_GAMES[i]);
        ImGui::Dummy(ImGui::CalcTextSize("C"));   // the marker column, so rows line up with the others
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        float start = ImGui::GetCursorPosX();
        ImGui::TextUnformatted(label);
        retro_ui_item_tooltip(piccolo_text(ui->strings, Str::setting_path_games_help));
        ImGui::SameLine(start + ui->label_width);

        if (settings_ui_folder_field(ui, settings_get_text(ui->settings, PICCOLO_PATH_GAMES[i]), nullptr,
                                     remove_width + ImGui::GetStyle().ItemSpacing.x))
        {
            window->choose_folder_key = PICCOLO_PATH_GAMES[i];
        }
        ImGui::SameLine(ImGui::GetContentRegionMax().x - remove_width);
        if (retro_ui_button("-"))
        {
            remove = static_cast<int32_t>(i);
        }
        retro_ui_item_tooltip(piccolo_text(ui->strings, Str::game_folder_remove_help));
        ImGui::PopID();
    }
    if (remove >= 0)
    {
        piccolo_settings_remove_game_folder(ui->settings, static_cast<uint32_t>(remove));
    }
    ImGui::Dummy(ImGui::CalcTextSize("C"));
    ImGui::SameLine();
    ImGui::BeginDisabled(set >= PICCOLO_GAME_FOLDER_MAX);
    ImGui::PushID("add_game_folder");
    if (retro_ui_button("+"))
    {
        window->choose_folder_key = PICCOLO_PATH_GAMES[set];
    }
    ImGui::PopID();
    ImGui::EndDisabled();
    retro_ui_item_tooltip(piccolo_text(ui->strings, Str::game_folder_add_help));
    if (!global)
    {
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            retro_ui_tooltip(piccolo_text(ui->strings, Str::global_only_help));
        }
    }
}

static void draw_general(SettingsWindow* window, const SettingsUiContext* ui, const SettingsWindowContext* context)
{
    settings_ui_row(ui, PICCOLO_LANGUAGE, nullptr);
    settings_ui_row(ui, PICCOLO_PAUSE_IN_MENU, nullptr);
    settings_ui_row(ui, PICCOLO_PAUSE_ON_FOCUS_LOSS, nullptr);
    settings_ui_row(ui, PICCOLO_PAUSE_ON_DISCONNECT, nullptr);
    settings_ui_row(ui, PICCOLO_SCREENSAVER_WHEN_PAUSED, nullptr);
    if (game_mode_supported())   // Linux only: nothing to turn on elsewhere
    {
        settings_ui_row(ui, PICCOLO_GAME_MODE, nullptr);
        ImGui::TextDisabled("%s", context->game_mode);
    }
    // FOLDERS: rarely changed, closed until opened. A click on a field
    // opens CHOOSE FOLDER (piccolo); RESET goes back to the default shown.
    if (folder_section(window, FolderSection::folders, piccolo_text(ui->strings, Str::section_folders), "folders"))
    {
        const char* keys[4] = {PICCOLO_PATH_CORES, PICCOLO_PATH_SYSTEM, PICCOLO_PATH_SAVES, PICCOLO_PATH_STATES};
        const char* hints[4] = {context->paths->cores.c_str(), context->paths->system.c_str(),
                                context->paths->saves.c_str(), context->paths->states.c_str()};
        // Android: no CORES row. Cores load only from the app's private
        // storage (shared storage is noexec); main.cpp ignores the setting.
#if defined(SDL_PLATFORM_ANDROID)
        uint32_t first = 1;
#else
        uint32_t first = 0;
#endif
        for (uint32_t i = first; i < 4; i++)
        {
            if (settings_ui_folder_row(ui, keys[i], hints[i]))
            {
                window->choose_folder_key = keys[i];
            }
        }
        ImGui::TextDisabled("%s", piccolo_text(ui->strings, Str::folders_next_game));
    }
    draw_game_folders(window, ui);
}

static long crop_at_scope(const SettingsUiContext* ui, const char* key)
{
    return std::strtol(settings_get_at(ui->settings, ui->scope, key, nullptr), nullptr, 10);
}

static void draw_picture(SettingsWindow* window, const SettingsUiContext* ui, const SettingsWindowContext* context)
{
    settings_ui_row(ui, PICCOLO_VIDEO_ASPECT, nullptr);
    settings_ui_row(ui, PICCOLO_VIDEO_SCALING, nullptr);
    settings_ui_row(ui, PICCOLO_VIDEO_MAX_OFFSCREEN, nullptr);
    settings_ui_row(ui, PICCOLO_VIDEO_FILTER, nullptr);
    bool choose_shader = false;
    settings_ui_path_row_with_button(ui, PICCOLO_VIDEO_SHADER_PRESET, piccolo_text(ui->strings, Str::shader_preset_none),
                                     piccolo_text(ui->strings, Str::shader_choose), &choose_shader);
    if (choose_shader)
    {
        window->choose_shader_request = true;
    }
    if (context->image != nullptr && !context->image->controls_available)
    {
        ImGui::TextDisabled("%s", piccolo_text(ui->strings, Str::shader_unavailable));
    }
    settings_ui_row(ui, PICCOLO_VIDEO_POSITION, nullptr);

    // The crop (and more image settings later) live in their own window.
    char crop[200] = {};
    std::snprintf(crop, sizeof(crop), "%s: %ld, %ld, %ld, %ld", piccolo_text(ui->strings, Str::crop_summary),
                  crop_at_scope(ui, PICCOLO_VIDEO_CROP_TOP), crop_at_scope(ui, PICCOLO_VIDEO_CROP_BOTTOM),
                  crop_at_scope(ui, PICCOLO_VIDEO_CROP_LEFT), crop_at_scope(ui, PICCOLO_VIDEO_CROP_RIGHT));
    ImGui::Spacing();
    char button[200] = {};
    if (retro_ui_button(with_id(button, sizeof(button), piccolo_text(ui->strings, Str::image_settings_button), "image")))
    {
        window->image_open = true;
    }
    if (context->image == nullptr || context->image->texture_id == 0)
    {
        retro_ui_item_tooltip(piccolo_text(ui->strings, Str::image_settings_button_help));
    }
    ImGui::TextDisabled("%s", crop);
}

static void draw_display(const SettingsUiContext* ui, const SettingsWindowContext* context)
{
    settings_ui_row(ui, PICCOLO_DISPLAY_FULLSCREEN, nullptr);
    settings_ui_row(ui, PICCOLO_DISPLAY_FULLSCREEN_TYPE, nullptr);
    retro_ui_separator_text(piccolo_text(ui->strings, Str::section_window));
    settings_ui_row(ui, PICCOLO_WINDOW_MODE, nullptr);
    if (settings_get_enum(ui->settings, PICCOLO_WINDOW_MODE) == PICCOLO_WINDOW_FIXED)
    {
        settings_ui_row(ui, PICCOLO_WINDOW_ASPECT, nullptr);
        settings_ui_row(ui, PICCOLO_WINDOW_HEIGHT, nullptr);
    }
    else
    {
        settings_ui_row(ui, PICCOLO_WINDOW_SCALE, nullptr);
    }
    ImGui::TextDisabled("%s", context->window_size);
    settings_ui_row(ui, PICCOLO_WINDOW_REMEMBER, nullptr);
}

// PRESET: sets the three latency settings below at the edited scope. Not a
// setting itself: it shows the preset the values match, or CUSTOM.
static void draw_latency_preset(const SettingsUiContext* ui)
{
    uint32_t current = piccolo_settings_latency_preset(ui->settings, ui->scope);
    ImGui::PushID("latency_preset");
    ImGui::Dummy(ImGui::CalcTextSize("C"));   // the marker column
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    float start = ImGui::GetCursorPosX();
    ImGui::TextUnformatted(piccolo_text(ui->strings, Str::latency_preset));
    retro_ui_item_tooltip(piccolo_text(ui->strings, Str::latency_preset_help));
    ImGui::SameLine(start + ui->label_width);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (retro_ui_begin_combo("##preset", piccolo_text(ui->strings, piccolo_settings_latency_preset_name(current))))
    {
        for (uint32_t i = 0; i < PICCOLO_LATENCY_PRESET_COUNT; i++)
        {
            ImGui::PushID(static_cast<int>(i));
            bool picked = ImGui::Selectable(piccolo_text(ui->strings, piccolo_settings_latency_preset_name(i)),
                                            i == current);
            ImGui::PopID();
            if (picked)
            {
                piccolo_settings_apply_latency_preset(ui->settings, ui->scope, i);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
}

static void draw_sync(SettingsWindow* window, const SettingsUiContext* ui, const SettingsWindowContext* context)
{
    settings_ui_row(ui, PICCOLO_VIDEO_TIMING, nullptr);
    settings_ui_row(ui, PICCOLO_VIDEO_CATCH_UP, nullptr);
    retro_ui_separator_text(piccolo_text(ui->strings, Str::section_low_latency));
    draw_latency_preset(ui);
    settings_ui_row(ui, PICCOLO_LATENCY_FRAMES_IN_FLIGHT, nullptr);
    settings_ui_row(ui, PICCOLO_LATENCY_WAIT_FOR_PRESENT, nullptr);

    // Frame delay only helps after waiting for present (see frame_delay() in
    // main.cpp): greyed out without it, the stored value kept.
    const char* waits = settings_get_at(ui->settings, ui->scope, PICCOLO_LATENCY_WAIT_FOR_PRESENT, nullptr);
    bool usable = waits != nullptr && std::strcmp(waits, "true") == 0;
    // AUTO ignores the manual value: greyed out then (kept for turning AUTO off).
    const char* automatic = settings_get_at(ui->settings, ui->scope, PICCOLO_LATENCY_FRAME_DELAY_AUTO, nullptr);
    bool is_automatic = automatic != nullptr && std::strcmp(automatic, "true") == 0;
    ImGui::BeginDisabled(!usable);
    ImGui::BeginDisabled(is_automatic);
    settings_ui_row(ui, PICCOLO_LATENCY_FRAME_DELAY, nullptr);
    ImGui::EndDisabled();
    if (!usable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        retro_ui_tooltip(piccolo_text(ui->strings, Str::frame_delay_needs_wait));
    }
    settings_ui_row(ui, PICCOLO_LATENCY_FRAME_DELAY_AUTO, nullptr);
    ImGui::EndDisabled();
    if (usable && is_automatic)
    {
        ImGui::TextDisabled("%s", context->frame_delay);
        if (context->frame_delay_found)
        {
            char button[200] = {};
            if (retro_ui_button(with_id(button, sizeof(button), piccolo_text(ui->strings, Str::frame_delay_search_again),
                                      "search_delay")))
            {
                window->search_frame_delay = true;
            }
            retro_ui_item_tooltip(piccolo_text(ui->strings, Str::frame_delay_search_again_help));
        }
    }
    ImGui::TextDisabled("%s", context->present_wait);
    settings_ui_row(ui, PICCOLO_LATENCY_RUN_AHEAD, nullptr);
    ImGui::TextDisabled("%s", context->run_ahead);
}

static void draw_sound(const SettingsUiContext* ui, const SettingsWindowContext* context)
{
    settings_ui_text_choice_row(ui, PICCOLO_AUDIO_DEVICE, context->audio_devices, context->audio_device_count,
                                piccolo_text(ui->strings, Str::audio_device_default),
                                piccolo_text(ui->strings, Str::audio_device_missing));
    settings_ui_row(ui, PICCOLO_AUDIO_VOLUME, nullptr);
    settings_ui_row(ui, PICCOLO_AUDIO_MUTE, nullptr);
    settings_ui_row(ui, PICCOLO_AUDIO_LATENCY, nullptr);
    ImGui::TextDisabled("%s", context->audio_latency);

    retro_ui_separator_text(piccolo_text(ui->strings, Str::section_advanced));
    settings_ui_row(ui, PICCOLO_AUDIO_RATE_CONTROL, nullptr);
    // The limit only matters with rate control on: greyed out without it, the stored value kept.
    const char* drc = settings_get_at(ui->settings, ui->scope, PICCOLO_AUDIO_RATE_CONTROL, nullptr);
    ImGui::BeginDisabled(drc == nullptr || std::strcmp(drc, "true") != 0);
    settings_ui_row(ui, PICCOLO_AUDIO_MAX_DEVIATION, nullptr);
    ImGui::EndDisabled();
    settings_ui_row(ui, PICCOLO_AUDIO_QUALITY, nullptr);
}

// One entry of THEME's or CUSTOM THEME's list.
struct ThemeChoice
{
    std::string           label;
    const RetroUiPalette* palette = nullptr;
};

static bool theme_choice_before(const ThemeChoice& a, const ThemeChoice& b)
{
    return a.label < b.label;
}

// The theme in `choices` the colors match: `preferred` (the one last picked
// or saved) when it does, since two themes may have the same colors, else
// the first. -1 if none.
static int32_t matching_theme(const Settings* settings, const std::vector<ThemeChoice>& choices, const std::string& preferred)
{
    int32_t found = -1;
    for (size_t i = 0; i < choices.size(); i++)
    {
        if (!piccolo_settings_ui_palette_is(settings, choices[i].palette))
        {
            continue;
        }
        if (choices[i].label == preferred)
        {
            return static_cast<int32_t>(i);
        }
        if (found < 0)
        {
            found = static_cast<int32_t>(i);
        }
    }
    return found;
}

// A theme row's label, then the value column's width for its combo.
static void theme_row_label(const SettingsUiContext* ui, Str label, Str help)
{
    ImGui::Dummy(ImGui::CalcTextSize("C"));   // the marker column
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    float start = ImGui::GetCursorPosX();
    ImGui::TextUnformatted(piccolo_text(ui->strings, label));
    retro_ui_item_tooltip(piccolo_text(ui->strings, help));
    ImGui::SameLine(start + ui->label_width);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
}

// THEME and CUSTOM THEME: they set every interface color. Not settings
// themselves: they show the theme the colors match.
//   THEME          the built-in themes and the ones that ship in themes/,
//                  sorted by name, then CUSTOM: CUSTOM when picked, or when
//                  the colors match none of them.
//   CUSTOM THEME   the user's own (themes/*-custom.cfg, SAVE THEME), only
//                  while THEME is CUSTOM: the one the colors match, or
//                  UNSAVED.
// The interface is global only.
static void draw_theme_preset(SettingsWindow* window, const SettingsUiContext* ui,
                              const std::vector<PiccoloThemeFile>* files)
{
    const Localization* strings = ui->strings;
    std::vector<ThemeChoice> shipped;
    std::vector<ThemeChoice> custom;
    for (uint32_t i = 0; i < static_cast<uint32_t>(RetroUiTheme::count); i++)
    {
        ThemeChoice choice = {};
        choice.label = piccolo_text(strings, piccolo_settings_ui_theme_name(static_cast<RetroUiTheme>(i)));
        choice.palette = retro_ui_theme_palette(static_cast<RetroUiTheme>(i));
        shipped.push_back(choice);
    }
    for (size_t i = 0; files != nullptr && i < files->size(); i++)
    {
        ThemeChoice choice = {};
        choice.label = (*files)[i].name;
        choice.palette = &(*files)[i].palette;
        if ((*files)[i].custom)
        {
            custom.push_back(choice);
        }
        else
        {
            shipped.push_back(choice);
        }
    }
    std::stable_sort(shipped.begin(), shipped.end(), theme_choice_before);
    std::stable_sort(custom.begin(), custom.end(), theme_choice_before);

    int32_t shipped_current = matching_theme(ui->settings, shipped, window->theme_choice);
    int32_t custom_current = matching_theme(ui->settings, custom, window->theme_choice);
    bool on_custom = window->theme_custom || shipped_current < 0;
    const char* custom_label = piccolo_text(strings, Str::preset_custom);

    bool global = ui->scope == SettingLayer::global;
    ImGui::PushID("ui_theme");
    ImGui::BeginDisabled(!global);

    theme_row_label(ui, Str::ui_theme, Str::ui_theme_help);
    const char* theme_label = on_custom ? custom_label : shipped[static_cast<size_t>(shipped_current)].label.c_str();
    if (retro_ui_begin_combo("##theme", theme_label))
    {
        for (size_t i = 0; i < shipped.size(); i++)
        {
            ImGui::PushID(static_cast<int>(i));
            bool picked = ImGui::Selectable(shipped[i].label.c_str(), !on_custom && static_cast<int32_t>(i) == shipped_current);
            ImGui::PopID();
            if (picked)
            {
                piccolo_settings_apply_ui_palette(ui->settings, shipped[i].palette);
                window->theme_choice = shipped[i].label;
                window->theme_custom = false;
            }
        }
        if (ImGui::Selectable(custom_label, on_custom))
        {
            window->theme_custom = true;   // the colors stay; CUSTOM THEME picks one
        }
        ImGui::EndCombo();
    }

    theme_row_label(ui, Str::ui_custom_theme, Str::ui_custom_theme_help);
    const char* custom_theme_label = "-";
    if (on_custom)
    {
        custom_theme_label = custom_current >= 0 ? custom[static_cast<size_t>(custom_current)].label.c_str()
                             : custom.empty()    ? piccolo_text(strings, Str::ui_custom_theme_none)
                                                 : piccolo_text(strings, Str::ui_custom_theme_unsaved);
    }
    ImGui::BeginDisabled(!on_custom || custom.empty());
    if (retro_ui_begin_combo("##custom_theme", custom_theme_label))
    {
        for (size_t i = 0; i < custom.size(); i++)
        {
            ImGui::PushID(static_cast<int>(i));
            bool picked = ImGui::Selectable(custom[i].label.c_str(), static_cast<int32_t>(i) == custom_current);
            ImGui::PopID();
            if (picked)
            {
                piccolo_settings_apply_ui_palette(ui->settings, custom[i].palette);
                window->theme_choice = custom[i].label;
                window->theme_custom = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();

    ImGui::EndDisabled();
    if (!global && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        retro_ui_tooltip(piccolo_text(strings, Str::global_only_help));
    }
    ImGui::PopID();
}

// SAVE THEME...: a button under CUSTOM THEME, then a popup: THEME picks
// which of the user's own themes to replace, or NEW, which asks for a NAME.
// SAVE leaves the name in window->save_theme_request for piccolo to write
// the file (a custom theme: it never replaces a shipped one).
static void draw_save_theme(SettingsWindow* window, const SettingsUiContext* ui, const std::vector<PiccoloThemeFile>* files)
{
    const Localization* strings = ui->strings;
    bool global = ui->scope == SettingLayer::global;

    // The user's own themes, sorted by name.
    std::vector<std::string> custom;
    for (size_t i = 0; files != nullptr && i < files->size(); i++)
    {
        if ((*files)[i].custom)
        {
            custom.push_back((*files)[i].name);
        }
    }
    std::sort(custom.begin(), custom.end());

    ImGui::PushID("ui_theme_save");
    ImGui::Dummy(ImGui::CalcTextSize("C"));   // the marker column
    ImGui::SameLine();
    float start = ImGui::GetCursorPosX();
    ImGui::SameLine(start + ui->label_width);
    ImGui::BeginDisabled(!global);
    if (retro_ui_button(piccolo_text(strings, Str::ui_theme_save)))
    {
        window->save_theme_open = true;
        window->save_theme_name[0] = '\0';
        // Start on the custom theme in use, else NEW.
        window->save_theme_target.clear();
        for (size_t i = 0; window->theme_custom && i < custom.size(); i++)
        {
            if (custom[i] == window->theme_choice)
            {
                window->save_theme_target = custom[i];
            }
        }
    }
    ImGui::EndDisabled();
    retro_ui_item_tooltip(piccolo_text(strings, Str::ui_theme_save_help));

    char title[128] = {};
    std::snprintf(title, sizeof(title), "%s###save_theme", piccolo_text(strings, Str::ui_theme_save_title));
    if (window->save_theme_open && !ImGui::IsPopupOpen(title))
    {
        ImGui::OpenPopup(title);
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    // One size, tall enough for everything it may show: the title, THEME,
    // NAME, the REPLACES line and the buttons.
    const ImGuiStyle& style = ImGui::GetStyle();
    float width = static_cast<float>(static_cast<int>(viewport->Size.x * 0.4f));
    float height = ImGui::GetFrameHeight() + style.WindowPadding.y * 2.0f + ImGui::GetFrameHeightWithSpacing() * 4.5f;   // half a row to spare for the edges
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, static_cast<float>(static_cast<int>(height))), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove))
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            window->save_theme_open = false;   // (B on a gamepad sends Escape while a popup is up)
        }
        // A target that is gone (deleted from the folder) is NEW again.
        bool target_found = window->save_theme_target.empty();
        for (size_t i = 0; i < custom.size(); i++)
        {
            target_found = target_found || custom[i] == window->save_theme_target;
        }
        if (!target_found)
        {
            window->save_theme_target.clear();
        }
        bool is_new = window->save_theme_target.empty();
        const char* new_label = piccolo_text(strings, Str::ui_theme_save_new);
        const char* theme_label = piccolo_text(strings, Str::ui_theme);
        const char* name_label = piccolo_text(strings, Str::ui_theme_save_name);
        float label_width = std::max(ImGui::CalcTextSize(theme_label).x, ImGui::CalcTextSize(name_label).x) +
                            ImGui::GetStyle().ItemSpacing.x * 2.0f;

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(theme_label);
        ImGui::SameLine(label_width);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (retro_ui_begin_combo("##target", is_new ? new_label : window->save_theme_target.c_str()))
        {
            if (ImGui::Selectable(new_label, is_new))
            {
                window->save_theme_target.clear();
            }
            for (size_t i = 0; i < custom.size(); i++)
            {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(custom[i].c_str(), custom[i] == window->save_theme_target))
                {
                    window->save_theme_target = custom[i];
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        is_new = window->save_theme_target.empty();

        std::string name = window->save_theme_target;
        bool enter = false;
        if (is_new)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(name_label);
            ImGui::SameLine(label_width);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
            }
            retro_ui_push_field_text();
            enter = ImGui::InputText("##name", window->save_theme_name, sizeof(window->save_theme_name),
                                     ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_EnterReturnsTrue);
            retro_ui_pop_field_text();
            // The name without spaces around it.
            name = window->save_theme_name;
            size_t first = name.find_first_not_of(' ');
            size_t last = name.find_last_not_of(' ');
            name = first != std::string::npos ? name.substr(first, last - first + 1) : std::string();
        }
        bool replaces = !is_new;
        for (size_t i = 0; i < custom.size(); i++)
        {
            replaces = replaces || name == custom[i];
        }
        if (replaces)
        {
            ImGui::TextDisabled("%s", piccolo_text(strings, Str::ui_theme_save_replaces));
        }

        bool can_save = !name.empty();
        ImGui::BeginDisabled(!can_save);
        if ((retro_ui_button(piccolo_text(strings, Str::ui_theme_save_button)) || enter) && can_save)
        {
            window->save_theme_request = name;
            window->save_theme_open = false;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (retro_ui_button(piccolo_text(strings, Str::map_cancel)))
        {
            window->save_theme_open = false;
        }
        if (!window->save_theme_open)
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

// The interface settings are global only, so this tab edits the global
// layer whatever the scope (on a core or game scope everything here would
// be disabled).
static void draw_interface(SettingsWindow* window, const SettingsUiContext* context,
                           const SettingsWindowContext* window_context)
{
    SettingsUiContext global = *context;
    global.scope = SettingLayer::global;
    const SettingsUiContext* ui = &global;
    draw_theme_preset(window, ui, window_context->themes);
    draw_save_theme(window, ui, window_context->themes);
    settings_ui_row(ui, PICCOLO_UI_MENU_BOXES, nullptr);
    settings_ui_row(ui, PICCOLO_UI_SHADOWS, nullptr);
    settings_ui_row(ui, PICCOLO_UI_THEME_CURSOR, nullptr);
    retro_ui_separator_text(piccolo_text(ui->strings, Str::section_colors));
    for (uint32_t i = 0; i < piccolo_settings_ui_color_count(); i++)
    {
        settings_ui_row(ui, piccolo_settings_ui_color_key(i), nullptr);
    }
}

// A tab's contents scroll on their own, under the scope and tabs, which stay
// put (the window itself doesn't scroll: a scrolling window with this layout
// jumped back to the top, as the core options list in the quick menu did).
static void begin_tab_content()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::BeginChild("content", ImVec2(0.0f, 0.0f));
}

static void end_tab_content()
{
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void draw_tabs(SettingsWindow* window, const SettingsUiContext* ui, const SettingsWindowContext* context)
{
    const Localization* strings = context->strings;
    char label[200] = {};
    if (!ImGui::BeginTabBar("tabs"))
    {
        return;
    }
    int target = retro_ui_tab_switch(static_cast<int>(window->tab), 5);
    if (target >= 0)
    {
        window->tab = static_cast<SettingsTab>(target);
        window->select_tab = true;
    }
    ImGuiTabItemFlags general_flags =
        window->select_tab && window->tab == SettingsTab::general ? ImGuiTabItemFlags_SetSelected : 0;
    ImGuiTabItemFlags interface_flags =
        window->select_tab && window->tab == SettingsTab::interface ? ImGuiTabItemFlags_SetSelected : 0;
    ImGuiTabItemFlags video_flags =
        window->select_tab && window->tab == SettingsTab::video ? ImGuiTabItemFlags_SetSelected : 0;
    ImGuiTabItemFlags sound_flags =
        window->select_tab && window->tab == SettingsTab::sound ? ImGuiTabItemFlags_SetSelected : 0;
    ImGuiTabItemFlags network_flags =
        window->select_tab && window->tab == SettingsTab::network ? ImGuiTabItemFlags_SetSelected : 0;
    window->select_tab = false;
    if (ImGui::BeginTabItem(with_id(label, sizeof(label), piccolo_text(strings, Str::tab_general), "general"), nullptr,
                            general_flags))
    {
        window->tab = SettingsTab::general;
        begin_tab_content();
        draw_general(window, ui, context);
        end_tab_content();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(with_id(label, sizeof(label), piccolo_text(strings, Str::tab_interface), "interface"), nullptr,
                            interface_flags))
    {
        window->tab = SettingsTab::interface;
        begin_tab_content();
        draw_interface(window, ui, context);
        end_tab_content();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(with_id(label, sizeof(label), piccolo_text(strings, Str::tab_video), "video"), nullptr,
                            video_flags))
    {
        window->tab = SettingsTab::video;
        if (ImGui::BeginTabBar("video_tabs"))
        {
            if (ImGui::BeginTabItem(with_id(label, sizeof(label), piccolo_text(strings, Str::tab_picture), "picture")))
            {
                begin_tab_content();
                draw_picture(window, ui, context);
                end_tab_content();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(with_id(label, sizeof(label), piccolo_text(strings, Str::tab_display), "display")))
            {
                begin_tab_content();
                draw_display(ui, context);
                end_tab_content();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(with_id(label, sizeof(label), piccolo_text(strings, Str::tab_sync), "sync")))
            {
                begin_tab_content();
                draw_sync(window, ui, context);
                end_tab_content();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(with_id(label, sizeof(label), piccolo_text(strings, Str::tab_sound), "sound"), nullptr,
                            sound_flags))
    {
        window->tab = SettingsTab::sound;
        begin_tab_content();
        draw_sound(ui, context);
        end_tab_content();
        ImGui::EndTabItem();
    }
    if (context->network != nullptr &&
        ImGui::BeginTabItem(with_id(label, sizeof(label), piccolo_text(strings, Str::net_title), "network"), nullptr,
                            network_flags))
    {
        window->tab = SettingsTab::network;
        begin_tab_content();
        net_window_draw_tab(context->network, ui->label_width);
        end_tab_content();
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}

void settings_window_draw(SettingsWindow* window, const SettingsWindowContext* context, ImFont* pixel_font,
                          int32_t scale)
{
    if (!window->open && !window->image_open)
    {
        return;
    }

    float block = static_cast<float>(scale);
    const ImGuiIO& io = ImGui::GetIO();
    if (window->open)
    {
        retro_ui_push_theme(pixel_font, scale);
        // 360 x 240 retro pixels, never bigger than the window.
        ImVec2 size(360.0f * block, 240.0f * block);
        size.x = size.x < io.DisplaySize.x - 8.0f * block ? size.x : io.DisplaySize.x - 8.0f * block;
        size.y = size.y < io.DisplaySize.y - 24.0f * block ? size.y : io.DisplaySize.y - 24.0f * block;
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing,
                                ImVec2(0.5f, 0.5f));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoScrollWithMouse;
        char title[200] = {};
        if (ImGui::Begin(with_id(title, sizeof(title), piccolo_text(context->strings, Str::settings_title), "settings"),
                         nullptr, flags))
        {
            retro_ui_begin_window(scale);
            if (retro_ui_close_button())
            {
                window->open = false;
            }
            draw_scope(window, context);

            SettingsUiContext ui = {};
            ui.settings = context->settings;
            ui.strings = context->strings;
            ui.scope = window->scope;
            ui.label_width = settings_ui_label_width(context->settings, context->strings);
            draw_tabs(window, &ui, context);
            retro_ui_end_window();
        }
        ImGui::End();
        retro_ui_pop_theme();
    }

    if (window->image_open)
    {
        SettingsUiContext ui = {};
        ui.settings = context->settings;
        ui.strings = context->strings;
        ui.scope = window->scope;
        retro_ui_push_theme(pixel_font, scale);
        ui.label_width = settings_ui_label_width(context->settings, context->strings);
        retro_ui_pop_theme();
        ImageWindowSource no_picture = {};
        image_window_draw(&window->image_open, &ui, context->image != nullptr ? context->image : &no_picture,
                          pixel_font, scale);
    }
}
