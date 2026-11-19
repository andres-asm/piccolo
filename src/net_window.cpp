#include "net_window.h"

#include <cstdio>

#include "imgui.h"

#include "piccolo_settings.h"
#include "piccolo_text.h"
#include "pixel_font_imgui.h"
#include "retro_ui.h"
#include "settings_ui.h"

void net_window_open(NetWindow* window)
{
    window->open = true;
}

// The code in two groups of three ("482 915"), big: it's read from across
// the room and typed on a phone.
static void draw_code(const NetAccess* access, ImFont* pixel_font, float block)
{
    char grouped[8] = {};
    std::snprintf(grouped, sizeof(grouped), "%.3s %.3s", access->code, access->code + 3);
    ImGui::PushFont(pixel_font, PIXEL_FONT_IMGUI_UNIT * block * 3.0f);
    retro_ui_push_section_text();
    ImGui::TextUnformatted(grouped);
    retro_ui_pop_section_text();
    ImGui::PopFont();
}

static void draw_pairing(const NetWindowContext* context, ImFont* pixel_font, float block)
{
    NetAccess* access = context->access;
    const Localization* strings = context->strings;
    if (!access->pairing_open)
    {
        if (retro_ui_button(piccolo_text(strings, Str::net_pair_button)))
        {
            net_access_open_pairing(access);
        }
        retro_ui_item_tooltip(piccolo_text(strings, Str::net_pair_button_help));
        return;
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(piccolo_text(strings, Str::net_pair_code));
    ImGui::SameLine();
    draw_code(access, pixel_font, block);
    ImGui::SameLine();
    uint32_t left = static_cast<uint32_t>(net_access_pairing_seconds_left(access));
    ImGui::Text("%u:%02u %s", left / 60, left % 60, piccolo_text(strings, Str::net_pair_left));
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::net_pair_new_code)))
    {
        net_access_open_pairing(access);   // already open: a new code
    }
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::net_pair_stop)))
    {
        net_access_close_pairing(access);
    }
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", piccolo_text(strings, Str::net_pair_instructions));
    ImGui::PopTextWrapPos();
}

static const Str ACCESS_LABELS[NET_ACCESS_LEVEL_COUNT] = {Str::net_access_gamepad, Str::net_access_stream,
                                                          Str::net_access_full};

// ACCESS: GAMEPAD / STREAM / FULL for one device (applied and saved at once).
static void draw_access(NetAccess* access, uint32_t device, const Localization* strings, float block)
{
    uint32_t current = static_cast<uint32_t>(access->trust.devices[device].access);
    ImGui::TextUnformatted(piccolo_text(strings, Str::net_device_access));
    retro_ui_item_tooltip(piccolo_text(strings, Str::net_device_access_help));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(56.0f * block);
    if (retro_ui_begin_combo("##access", piccolo_text(strings, ACCESS_LABELS[current])))
    {
        for (uint32_t level = 0; level < NET_ACCESS_LEVEL_COUNT; level++)
        {
            ImGui::PushID(static_cast<int>(level));
            bool picked = ImGui::Selectable(piccolo_text(strings, ACCESS_LABELS[level]), level == current);
            ImGui::PopID();
            if (picked && level != current)
            {
                net_access_set_level(access, device, static_cast<NetAccessLevel>(level));
            }
        }
        ImGui::EndCombo();
    }
    retro_ui_item_tooltip(piccolo_text(strings, Str::net_device_access_help));
}

static void draw_devices(const NetWindowContext* context, float block)
{
    NetAccess* access = context->access;
    const Localization* strings = context->strings;
    if (access->trust.devices.empty())
    {
        ImGui::TextDisabled("%s", piccolo_text(strings, Str::net_no_devices));
        return;
    }
    // FORGET changes the list: act after drawing it.
    int32_t forget = -1;
    for (size_t i = 0; i < access->trust.devices.size(); i++)
    {
        const NetTrustedDevice* device = &access->trust.devices[i];
        ImGui::PushID(static_cast<int>(i));
        char fingerprint[32];
        net_key_fingerprint(device->key, fingerprint, sizeof(fingerprint));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(device->name.empty() ? "?" : device->name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s  %s", fingerprint, device->paired.c_str());
        ImGui::SameLine();
        draw_access(access, static_cast<uint32_t>(i), strings, block);
        if (net_access_device_connected(access, static_cast<uint32_t>(i)))
        {
            ImGui::SameLine();
            retro_ui_push_section_text();
            ImGui::TextUnformatted(piccolo_text(strings, Str::net_device_connected));
            retro_ui_pop_section_text();
        }
        ImGui::SameLine();
        if (retro_ui_button(piccolo_text(strings, Str::net_device_forget)))
        {
            forget = static_cast<int32_t>(i);
        }
        retro_ui_item_tooltip(piccolo_text(strings, Str::net_device_forget_help));
        ImGui::PopID();
    }
    if (forget >= 0)
    {
        net_access_remove_device(access, static_cast<uint32_t>(forget));
    }
}

void net_window_draw(NetWindow* window, const NetWindowContext* context, ImFont* pixel_font, int32_t scale)
{
    if (!window->open)
    {
        return;
    }
    float block = static_cast<float>(scale);
    const ImGuiIO& io = ImGui::GetIO();
    retro_ui_push_theme(pixel_font, scale);
    float width = 300.0f * block;
    width = width < io.DisplaySize.x - 8.0f * block ? width : io.DisplaySize.x - 8.0f * block;
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_AlwaysAutoResize;
    const Localization* strings = context->strings;
    char title[200] = {};
    std::snprintf(title, sizeof(title), "%s###network", piccolo_text(strings, Str::net_title));
    if (ImGui::Begin(title, nullptr, flags))
    {
        retro_ui_begin_window(scale);
        if (retro_ui_close_button())
        {
            window->open = false;
        }
        SettingsUiContext rows;
        rows.settings = context->settings;
        rows.strings = strings;
        rows.scope = SettingLayer::global;
        rows.label_width = 90.0f * block;
        settings_ui_row(&rows, PICCOLO_NET_ENABLED, nullptr);
        settings_ui_row(&rows, PICCOLO_NET_NAME, nullptr);
        settings_ui_row(&rows, PICCOLO_NET_PORT, nullptr);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(piccolo_text(strings, Str::net_this_device));
        retro_ui_item_tooltip(piccolo_text(strings, Str::net_this_device_help));
        ImGui::SameLine(rows.label_width + ImGui::GetStyle().WindowPadding.x);
        ImGui::TextUnformatted(context->this_device[0] != '\0' ? context->this_device : "-");

        if (context->access == nullptr)
        {
            ImGui::Spacing();
            ImGui::TextDisabled("%s", piccolo_text(strings, context->listen_failed ? Str::net_listen_failed : Str::net_off));
        }
        else
        {
            retro_ui_separator_text(piccolo_text(strings, Str::net_section_pairing));
            draw_pairing(context, pixel_font, block);
            retro_ui_separator_text(piccolo_text(strings, Str::net_section_devices));
            draw_devices(context, block);
        }
        retro_ui_end_window();
    }
    ImGui::End();
    retro_ui_pop_theme();
}
