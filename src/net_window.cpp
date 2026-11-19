#include "net_window.h"

#include <cstdio>

#include "imgui.h"

#include "piccolo_settings.h"
#include "piccolo_text.h"
#include "retro_ui.h"
#include "settings_ui.h"

// The code in two groups of three ("482 915"), in the highlight color.
static void draw_code(const NetAccess* access)
{
    char grouped[8] = {};
    std::snprintf(grouped, sizeof(grouped), "%.3s %.3s", access->code, access->code + 3);
    retro_ui_push_section_text();
    ImGui::TextUnformatted(grouped);
    retro_ui_pop_section_text();
}

//   CODE  482 915
//   VALIDITY: 4:32  [REFRESH] [STOP]
//   Type this code on the other device...
static void draw_pairing(const NetWindowContext* context)
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
    ImGui::TextUnformatted(piccolo_text(strings, Str::net_pair_code));
    ImGui::SameLine();
    draw_code(access);
    uint32_t left = static_cast<uint32_t>(net_access_pairing_seconds_left(access));
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s: %u:%02u", piccolo_text(strings, Str::net_pair_validity), left / 60, left % 60);
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::net_pair_refresh)))
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

// ACCESS: GAMEPAD / STREAM / FULL for one device, as a row of buttons
// (applied and saved at once).
static void draw_access(NetAccess* access, uint32_t device, const Localization* strings)
{
    const char* labels[NET_ACCESS_LEVEL_COUNT];
    for (uint32_t level = 0; level < NET_ACCESS_LEVEL_COUNT; level++)
    {
        labels[level] = piccolo_text(strings, ACCESS_LABELS[level]);
    }
    uint32_t current = static_cast<uint32_t>(access->trust.devices[device].access);
    if (retro_ui_choice_buttons("##access", labels, NET_ACCESS_LEVEL_COUNT, &current))
    {
        net_access_set_level(access, device, static_cast<NetAccessLevel>(current));
    }
    retro_ui_item_tooltip(piccolo_text(strings, Str::net_device_access_help));
}

// The ping of a connected device's first connection, or false.
static bool device_ping(const NetAccess* access, uint32_t device, NetLinkStats* stats)
{
    for (uint32_t peer = 0; peer < NET_MAX_PEERS; peer++)
    {
        if (access->peers[peer].state == NetAccessPeerState::authorized &&
            access->peers[peer].device == static_cast<int32_t>(device))
        {
            return net_access_peer_stats(access, peer, stats);
        }
    }
    return false;
}

//   ANA'S PHONE  9c77-3332-311c-4017  CONNECTED 4 MS
//   [GAMEPAD][STREAM][FULL]  [FORGET]
static void draw_devices(const NetWindowContext* context)
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
        if (i > 0)
        {
            ImGui::Spacing();
        }
        char fingerprint[32];
        net_key_fingerprint(device->key, fingerprint, sizeof(fingerprint));
        ImGui::TextUnformatted(device->name.empty() ? "?" : device->name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s", fingerprint);
        if (net_access_device_connected(access, static_cast<uint32_t>(i)))
        {
            ImGui::SameLine();
            retro_ui_push_section_text();
            ImGui::TextUnformatted(piccolo_text(strings, Str::net_device_connected));
            retro_ui_pop_section_text();
            NetLinkStats stats;
            if (device_ping(access, static_cast<uint32_t>(i), &stats))
            {
                // The round trip in milliseconds, as ENet measures it.
                ImGui::SameLine();
                ImGui::TextDisabled("%u MS", stats.round_trip_ms);
                retro_ui_item_tooltip(piccolo_text(strings, Str::net_device_ping_help));
            }
        }
        draw_access(access, static_cast<uint32_t>(i), strings);
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

void net_window_draw_tab(const NetWindowContext* context, float label_width)
{
    const Localization* strings = context->strings;
    SettingsUiContext rows;
    rows.settings = context->settings;
    rows.strings = strings;
    rows.scope = SettingLayer::global;
    rows.label_width = label_width;
    settings_ui_row(&rows, PICCOLO_NET_ENABLED, nullptr);
    settings_ui_row(&rows, PICCOLO_NET_NAME, nullptr);
    settings_ui_row(&rows, PICCOLO_NET_PORT, nullptr);

    // As a row: the marker column, then the label column.
    ImGui::Dummy(ImGui::CalcTextSize("C"));
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    float start = ImGui::GetCursorPosX();
    ImGui::TextUnformatted(piccolo_text(strings, Str::net_this_device));
    retro_ui_item_tooltip(piccolo_text(strings, Str::net_this_device_help));
    ImGui::SameLine(start + label_width);
    ImGui::TextUnformatted(context->this_device[0] != '\0' ? context->this_device : "-");

    if (context->access == nullptr)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", piccolo_text(strings, context->listen_failed ? Str::net_listen_failed : Str::net_off));
        return;
    }
    retro_ui_separator_text(piccolo_text(strings, Str::net_section_pairing));
    draw_pairing(context);
    retro_ui_separator_text(piccolo_text(strings, Str::net_section_devices));
    draw_devices(context);
}
