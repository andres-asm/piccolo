#include "settings_window.h"

#include <cstdio>
#include <cstdlib>

#include "imgui.h"

#include "piccolo_settings.h"
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

// The widest label of the rows with a label column (not checkboxes).
static float label_column_width(const Settings* settings)
{
    float widest = 0.0f;
    for (uint32_t i = 0; i < settings->descriptor_count; i++)
    {
        const SettingDescriptor* descriptor = &settings->descriptors[i];
        if (descriptor->type == SettingType::boolean || descriptor->label == nullptr)
        {
            continue;
        }
        float width = ImGui::CalcTextSize(descriptor->label).x;
        widest = width > widest ? width : widest;
    }
    return widest + ImGui::GetStyle().ItemSpacing.x * 2.0f;
}

// GLOBAL / THIS CORE / THIS GAME, only the ones that exist right now.
static void draw_scope(SettingsWindow* window, const SettingsWindowContext* context)
{
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
        ImGui::TextDisabled("SCOPE: GLOBAL");
        ImGui::SetItemTooltip("START A GAME TO CHANGE SETTINGS FOR ONE CORE OR ONE GAME");
        return;
    }

    char core_label[160] = {};
    char game_label[400] = {};
    std::snprintf(core_label, sizeof(core_label), "THIS CORE (%s)", context->core_name);
    std::snprintf(game_label, sizeof(game_label), "THIS GAME (%s)", context->game_name);
    const char* preview = window->scope == SettingLayer::core   ? core_label
                          : window->scope == SettingLayer::game ? game_label
                                                                : "GLOBAL";
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("SCOPE");
    ImGui::SetItemTooltip("WHERE CHANGES GO. A CORE OR GAME SETTING WINS OVER THE GLOBAL ONE.");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (retro_ui_begin_combo("##scope", preview))
    {
        if (ImGui::Selectable("GLOBAL", window->scope == SettingLayer::global))
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
    settings_ui_row(ui, PICCOLO_PAUSE_IN_MENU, nullptr);
    settings_ui_row(ui, PICCOLO_PAUSE_ON_FOCUS_LOSS, nullptr);
    settings_ui_row(ui, PICCOLO_SCREENSAVER_WHEN_PAUSED, nullptr);
    ImGui::SeparatorText("FOLDERS");
    settings_ui_row(ui, PICCOLO_PATH_CORES, context->paths->cores.c_str());
    settings_ui_row(ui, PICCOLO_PATH_SYSTEM, context->paths->system.c_str());
    settings_ui_row(ui, PICCOLO_PATH_SAVES, context->paths->saves.c_str());
    settings_ui_row(ui, PICCOLO_PATH_STATES, context->paths->states.c_str());
    ImGui::TextDisabled("FOLDER CHANGES APPLY TO THE NEXT GAME");
}

static void draw_picture(SettingsWindow* window, const SettingsUiContext* ui, const SettingsWindowContext* context)
{
    settings_ui_row(ui, PICCOLO_VIDEO_ASPECT, nullptr);
    settings_ui_row(ui, PICCOLO_VIDEO_SCALING, nullptr);
    settings_ui_row(ui, PICCOLO_VIDEO_MAX_OFFSCREEN, nullptr);
    settings_ui_row(ui, PICCOLO_VIDEO_FILTER, nullptr);
    settings_ui_row(ui, PICCOLO_VIDEO_POSITION, nullptr);

    // The crop (and more image settings later) live in their own window.
    char crop[96] = {};
    std::snprintf(crop, sizeof(crop), "CROP: T %ld  B %ld  L %ld  R %ld",
                  std::strtol(settings_get_at(ui->settings, ui->scope, PICCOLO_VIDEO_CROP_TOP, nullptr), nullptr, 10),
                  std::strtol(settings_get_at(ui->settings, ui->scope, PICCOLO_VIDEO_CROP_BOTTOM, nullptr), nullptr, 10),
                  std::strtol(settings_get_at(ui->settings, ui->scope, PICCOLO_VIDEO_CROP_LEFT, nullptr), nullptr, 10),
                  std::strtol(settings_get_at(ui->settings, ui->scope, PICCOLO_VIDEO_CROP_RIGHT, nullptr), nullptr, 10));
    ImGui::Spacing();
    if (ImGui::Button("IMAGE SETTINGS..."))
    {
        window->image_open = true;
    }
    if (context->image == nullptr || context->image->texture_id == 0)
    {
        ImGui::SetItemTooltip("BEST WITH A GAME RUNNING: IT SHOWS THE PICTURE");
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", crop);
}

static void draw_display(const SettingsUiContext* ui, const SettingsWindowContext* context)
{
    settings_ui_row(ui, PICCOLO_DISPLAY_FULLSCREEN, nullptr);
    settings_ui_row(ui, PICCOLO_DISPLAY_FULLSCREEN_TYPE, nullptr);
    ImGui::SeparatorText("WINDOW");
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
    ImGui::TextUnformatted("PRESET");
    ImGui::SetItemTooltip("Sets the three settings below. Change them one by one for anything in between.");
    ImGui::SameLine(start + ui->label_width);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (retro_ui_begin_combo("##preset", piccolo_settings_latency_preset_name(current)))
    {
        for (uint32_t i = 0; i < PICCOLO_LATENCY_PRESET_COUNT; i++)
        {
            if (ImGui::Selectable(piccolo_settings_latency_preset_name(i), i == current))
            {
                piccolo_settings_apply_latency_preset(ui->settings, ui->scope, i);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
}

static void draw_sync(const SettingsUiContext* ui, const SettingsWindowContext* context)
{
    settings_ui_row(ui, PICCOLO_VIDEO_TIMING, nullptr);
    ImGui::SeparatorText("LOW LATENCY");
    draw_latency_preset(ui);
    settings_ui_row(ui, PICCOLO_LATENCY_FRAMES_IN_FLIGHT, nullptr);
    settings_ui_row(ui, PICCOLO_LATENCY_WAIT_FOR_PRESENT, nullptr);
    settings_ui_row(ui, PICCOLO_LATENCY_FRAME_DELAY, nullptr);
    ImGui::TextDisabled("%s", context->present_wait);
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
        // 360 x 230 retro pixels, never bigger than the window.
        ImVec2 size(360.0f * block, 230.0f * block);
        size.x = size.x < io.DisplaySize.x - 8.0f * block ? size.x : io.DisplaySize.x - 8.0f * block;
        size.y = size.y < io.DisplaySize.y - 24.0f * block ? size.y : io.DisplaySize.y - 24.0f * block;
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing,
                                ImVec2(0.5f, 0.5f));

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("SETTINGS", nullptr, flags))
        {
            retro_ui_begin_window(scale);
            if (retro_ui_close_button())
            {
                window->open = false;
            }
            draw_scope(window, context);

            SettingsUiContext ui = {};
            ui.settings = context->settings;
            ui.scope = window->scope;
            ui.label_width = label_column_width(context->settings);

            if (ImGui::BeginTabBar("tabs"))
            {
                ImGuiTabItemFlags general_flags =
                    window->select_tab && window->tab == SettingsTab::general ? ImGuiTabItemFlags_SetSelected : 0;
                ImGuiTabItemFlags video_flags =
                    window->select_tab && window->tab == SettingsTab::video ? ImGuiTabItemFlags_SetSelected : 0;
                window->select_tab = false;
                if (ImGui::BeginTabItem("GENERAL", nullptr, general_flags))
                {
                    window->tab = SettingsTab::general;
                    draw_general(&ui, context);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("VIDEO", nullptr, video_flags))
                {
                    window->tab = SettingsTab::video;
                    if (ImGui::BeginTabBar("video_tabs"))
                    {
                        if (ImGui::BeginTabItem("PICTURE"))
                        {
                            draw_picture(window, &ui, context);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("DISPLAY"))
                        {
                            draw_display(&ui, context);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("SYNC"))
                        {
                            draw_sync(&ui, context);
                            ImGui::EndTabItem();
                        }
                        ImGui::EndTabBar();
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            retro_ui_end_window();
        }
        ImGui::End();
        retro_ui_pop_theme();
    }

    if (window->image_open)
    {
        SettingsUiContext ui = {};
        ui.settings = context->settings;
        ui.scope = window->scope;
        retro_ui_push_theme(pixel_font, scale);
        ui.label_width = label_column_width(context->settings);
        retro_ui_pop_theme();
        ImageWindowSource no_picture = {};
        image_window_draw(&window->image_open, &ui, context->image != nullptr ? context->image : &no_picture,
                          pixel_font, scale);
    }
}
