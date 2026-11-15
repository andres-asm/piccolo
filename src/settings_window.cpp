#include "settings_window.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <SDL3/SDL.h>

#include "imgui.h"

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

// GLOBAL / THIS CORE / THIS GAME, only the ones that exist right now.
static void draw_scope(SettingsWindow* window, const SettingsWindowContext* context)
{
    const Localization* strings = context->strings;
    bool has_core = context->core_name[0] != '\0';
    bool has_game = has_core && context->game_name[0] != '\0';
    if (window->choose_scope)
    {
        // The most specific layer already in use, so changes keep going there.
        window->choose_scope = false;
        window->scope = SettingLayer::global;
        if (has_game && settings_layer_has_values(context->settings, SettingLayer::game))
        {
            window->scope = SettingLayer::game;
        }
        else if (has_core && settings_layer_has_values(context->settings, SettingLayer::core))
        {
            window->scope = SettingLayer::core;
        }
    }
    if ((window->scope == SettingLayer::core && !has_core) || (window->scope == SettingLayer::game && !has_game))
    {
        window->scope = SettingLayer::global;   // the game stopped
    }
    if (!has_core)
    {
        ImGui::TextDisabled("%s: %s", piccolo_text(strings, Str::scope), piccolo_text(strings, Str::scope_global));
        retro_ui_item_tooltip(piccolo_text(strings, Str::scope_start_game_help));
        return;
    }

    char core_label[400] = {};
    char game_label[600] = {};
    std::snprintf(core_label, sizeof(core_label), "%s (%s)###core", piccolo_text(strings, Str::scope_core),
                  context->core_name);
    std::snprintf(game_label, sizeof(game_label), "%s (%s)###game", piccolo_text(strings, Str::scope_game),
                  context->game_name);
    char global_label[200] = {};
    with_id(global_label, sizeof(global_label), piccolo_text(strings, Str::scope_global), "global");
    // The combo shows the label without its "###" part.
    const char* preview = window->scope == SettingLayer::core   ? core_label
                          : window->scope == SettingLayer::game ? game_label
                                                                : global_label;
    char preview_text[600] = {};
    std::snprintf(preview_text, sizeof(preview_text), "%s", preview);
    char* hidden = std::strstr(preview_text, "###");
    if (hidden != nullptr)
    {
        *hidden = '\0';
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(piccolo_text(strings, Str::scope));
    retro_ui_item_tooltip(piccolo_text(strings, Str::scope_help));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (retro_ui_begin_combo("##scope", preview_text))
    {
        if (ImGui::Selectable(global_label, window->scope == SettingLayer::global))
        {
            window->scope = SettingLayer::global;
        }
        if (ImGui::Selectable(core_label, window->scope == SettingLayer::core))
        {
            window->scope = SettingLayer::core;
        }
        if (has_game && ImGui::Selectable(game_label, window->scope == SettingLayer::game))
        {
            window->scope = SettingLayer::game;
        }
        ImGui::EndCombo();
    }
}

static void draw_general(const SettingsUiContext* ui, const SettingsWindowContext* context)
{
    settings_ui_row(ui, PICCOLO_LANGUAGE, nullptr);
    settings_ui_row(ui, PICCOLO_PAUSE_IN_MENU, nullptr);
    settings_ui_row(ui, PICCOLO_PAUSE_ON_FOCUS_LOSS, nullptr);
    settings_ui_row(ui, PICCOLO_SCREENSAVER_WHEN_PAUSED, nullptr);
    settings_ui_row(ui, PICCOLO_GAME_MODE, nullptr);
    ImGui::TextDisabled("%s", context->game_mode);
    retro_ui_separator_text(piccolo_text(ui->strings, Str::section_folders));
    settings_ui_row(ui, PICCOLO_PATH_CORES, context->paths->cores.c_str());
    settings_ui_row(ui, PICCOLO_PATH_SYSTEM, context->paths->system.c_str());
    settings_ui_row(ui, PICCOLO_PATH_SAVES, context->paths->saves.c_str());
    settings_ui_row(ui, PICCOLO_PATH_STATES, context->paths->states.c_str());
    ImGui::TextDisabled("%s", piccolo_text(ui->strings, Str::folders_next_game));
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

// THEME: sets every interface color. Not a setting itself: it shows the
// theme the colors match, or CUSTOM. The interface is global only.
static void draw_theme_preset(const SettingsUiContext* ui)
{
    RetroUiTheme current = piccolo_settings_ui_theme(ui->settings);
    bool global = ui->scope == SettingLayer::global;
    ImGui::PushID("ui_theme");
    ImGui::Dummy(ImGui::CalcTextSize("C"));   // the marker column
    ImGui::SameLine();
    ImGui::BeginDisabled(!global);
    ImGui::AlignTextToFramePadding();
    float start = ImGui::GetCursorPosX();
    ImGui::TextUnformatted(piccolo_text(ui->strings, Str::ui_theme));
    retro_ui_item_tooltip(piccolo_text(ui->strings, Str::ui_theme_help));
    ImGui::SameLine(start + ui->label_width);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (retro_ui_begin_combo("##theme", piccolo_text(ui->strings, piccolo_settings_ui_theme_name(current))))
    {
        for (uint32_t i = 0; i < static_cast<uint32_t>(RetroUiTheme::count); i++)
        {
            RetroUiTheme theme = static_cast<RetroUiTheme>(i);
            ImGui::PushID(static_cast<int>(i));
            bool picked = ImGui::Selectable(piccolo_text(ui->strings, piccolo_settings_ui_theme_name(theme)), theme == current);
            ImGui::PopID();
            if (picked)
            {
                piccolo_settings_apply_ui_theme(ui->settings, theme);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if (!global && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        retro_ui_tooltip(piccolo_text(ui->strings, Str::global_only_help));
    }
    ImGui::PopID();
}

// The interface settings are global only, so this tab edits the global
// layer whatever the scope (on a core or game scope everything here would
// be disabled).
static void draw_interface(const SettingsUiContext* context)
{
    SettingsUiContext global = *context;
    global.scope = SettingLayer::global;
    const SettingsUiContext* ui = &global;
    draw_theme_preset(ui);
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
    ImGuiTabItemFlags general_flags =
        window->select_tab && window->tab == SettingsTab::general ? ImGuiTabItemFlags_SetSelected : 0;
    ImGuiTabItemFlags interface_flags =
        window->select_tab && window->tab == SettingsTab::interface ? ImGuiTabItemFlags_SetSelected : 0;
    ImGuiTabItemFlags video_flags =
        window->select_tab && window->tab == SettingsTab::video ? ImGuiTabItemFlags_SetSelected : 0;
    ImGuiTabItemFlags sound_flags =
        window->select_tab && window->tab == SettingsTab::sound ? ImGuiTabItemFlags_SetSelected : 0;
    window->select_tab = false;
    if (ImGui::BeginTabItem(with_id(label, sizeof(label), piccolo_text(strings, Str::tab_general), "general"), nullptr,
                            general_flags))
    {
        window->tab = SettingsTab::general;
        begin_tab_content();
        draw_general(ui, context);
        end_tab_content();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(with_id(label, sizeof(label), piccolo_text(strings, Str::tab_interface), "interface"), nullptr,
                            interface_flags))
    {
        window->tab = SettingsTab::interface;
        begin_tab_content();
        draw_interface(ui);
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
