#include "settings_ui.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "imgui.h"

#include "libretro_host.h"   // LIBRETRO_PATH_MAX
#include "retro_ui.h"

// Marker colors: our own, distinct from the theme's blues (0xAARRGGBB).
constexpr uint32_t MARKER_CORE = 0xFFF0C040u;   // amber
constexpr uint32_t MARKER_GAME = 0xFF60E080u;   // green

static ImVec4 color_of(uint32_t argb)
{
    return ImVec4(static_cast<float>((argb >> 16) & 0xFFu) / 255.0f, static_cast<float>((argb >> 8) & 0xFFu) / 255.0f,
                  static_cast<float>(argb & 0xFFu) / 255.0f, static_cast<float>(argb >> 24) / 255.0f);
}

const char* settings_ui_scope_name(SettingLayer scope)
{
    switch (scope)
    {
    case SettingLayer::defaults:
    case SettingLayer::global:
        return "GLOBAL";
    case SettingLayer::core:
        return "THIS CORE";
    case SettingLayer::game:
        return "THIS GAME";
    }
    return "GLOBAL";
}

void settings_ui_marker(const Settings* settings, const char* key)
{
    SettingLayer source = SettingLayer::defaults;
    settings_get(settings, key, &source);
    ImVec2 size = ImGui::CalcTextSize("C");
    if (source != SettingLayer::core && source != SettingLayer::game)
    {
        ImGui::Dummy(size);
        return;
    }
    bool core = source == SettingLayer::core;
    ImGui::TextColored(color_of(core ? MARKER_CORE : MARKER_GAME), "%s", core ? "C" : "G");
    ImGui::SetItemTooltip("%s", core ? "SET FOR THIS CORE" : "SET FOR THIS GAME");
}

bool settings_ui_reset_button(const SettingsUiContext* context, const char* key)
{
    if (settings_get_layer(context->settings, context->scope, key) == nullptr)
    {
        return false;
    }
    float width = ImGui::CalcTextSize("RESET").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::SameLine(ImGui::GetContentRegionMax().x - width);
    bool clicked = ImGui::Button("RESET");
    ImGui::SetItemTooltip("%s", context->scope == SettingLayer::global ? "BACK TO THE DEFAULT"
                                                                       : "BACK TO THE VALUE BELOW (STOP OVERRIDING)");
    if (clicked)
    {
        settings_reset(context->settings, context->scope, key);
    }
    return clicked;
}

// The label column: text, then the cursor moves to the widget column.
static void label_column(const SettingsUiContext* context, const SettingDescriptor* descriptor)
{
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    float start = ImGui::GetCursorPosX();
    ImGui::TextUnformatted(descriptor->label);
    if (descriptor->help != nullptr)
    {
        ImGui::SetItemTooltip("%s", descriptor->help);
    }
    ImGui::SameLine(start + context->label_width);
}

// Room left for the widget, keeping space for the reset button.
static float widget_width()
{
    float reset = ImGui::CalcTextSize("RESET").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    return ImGui::GetContentRegionAvail().x - reset - ImGui::GetStyle().ItemSpacing.x;
}

static bool enum_widget(const SettingsUiContext* context, const SettingDescriptor* descriptor, const char* value)
{
    uint32_t current = 0;
    for (uint32_t i = 0; i < descriptor->value_count; i++)
    {
        if (std::strcmp(descriptor->values[i].key, value) == 0)
        {
            current = i;
        }
    }
    bool changed = false;
    ImGui::SetNextItemWidth(widget_width());
    if (retro_ui_begin_combo("##value", descriptor->values[current].label))
    {
        for (uint32_t i = 0; i < descriptor->value_count; i++)
        {
            if (ImGui::Selectable(descriptor->values[i].label, i == current) && i != current)
            {
                changed = settings_set_enum(context->settings, context->scope, descriptor->key, i);
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

static bool integer_widget(const SettingsUiContext* context, const SettingDescriptor* descriptor, const char* value)
{
    int number = static_cast<int>(std::strtol(value, nullptr, 10));
    ImGui::SetNextItemWidth(widget_width());
    if (!ImGui::InputInt("##value", &number, descriptor->step > 0 ? descriptor->step : 1, 8))
    {
        return false;
    }
    number = number < descriptor->min ? descriptor->min : (number > descriptor->max ? descriptor->max : number);
    return settings_set_int(context->settings, context->scope, descriptor->key, static_cast<int32_t>(number));
}

static bool real_widget(const SettingsUiContext* context, const SettingDescriptor* descriptor, const char* value)
{
    float number = std::strtof(value, nullptr);
    ImGui::SetNextItemWidth(widget_width());
    if (!ImGui::SliderFloat("##value", &number, static_cast<float>(descriptor->min_real),
                            static_cast<float>(descriptor->max_real), "%.1f%%"))
    {
        return false;
    }
    double rounded = static_cast<double>(number);
    if (descriptor->step_real > 0.0)
    {
        rounded = std::round(rounded / descriptor->step_real) * descriptor->step_real;
    }
    return settings_set_real(context->settings, context->scope, descriptor->key, rounded);
}

static bool path_widget(const SettingsUiContext* context, const SettingDescriptor* descriptor, const char* value,
                        const char* hint)
{
    // ImGui keeps its own copy of the text while the field is being edited;
    // this buffer only seeds it. The value is set when editing ends.
    char buffer[LIBRETRO_PATH_MAX] = {};
    std::snprintf(buffer, sizeof(buffer), "%s", value);
    ImGui::SetNextItemWidth(widget_width());
    ImGui::InputTextWithHint("##value", hint != nullptr ? hint : "", buffer, sizeof(buffer));
    if (!ImGui::IsItemDeactivatedAfterEdit())
    {
        return false;
    }
    if (buffer[0] == '\0')
    {
        return settings_reset(context->settings, context->scope, descriptor->key);   // empty = default
    }
    return settings_set(context->settings, context->scope, descriptor->key, buffer);
}

bool settings_ui_row(const SettingsUiContext* context, const char* key, const char* hint)
{
    const SettingDescriptor* descriptor = settings_find(context->settings, key);
    if (descriptor == nullptr)
    {
        return false;
    }
    bool global_only_here = !descriptor->overridable && context->scope != SettingLayer::global;
    const char* value = settings_get_at(context->settings, context->scope, key, nullptr);
    bool changed = false;

    ImGui::PushID(key);
    settings_ui_marker(context->settings, key);
    ImGui::BeginDisabled(global_only_here);
    if (descriptor->type == SettingType::boolean)
    {
        ImGui::SameLine();
        bool checked = std::strcmp(value, "true") == 0;
        if (retro_ui_checkbox(descriptor->label, &checked))
        {
            changed = settings_set_bool(context->settings, context->scope, key, checked);
        }
        if (descriptor->help != nullptr)
        {
            ImGui::SetItemTooltip("%s", descriptor->help);
        }
    }
    else
    {
        label_column(context, descriptor);
        switch (descriptor->type)
        {
        case SettingType::enumeration:
            changed = enum_widget(context, descriptor, value);
            break;
        case SettingType::integer:
            changed = integer_widget(context, descriptor, value);
            break;
        case SettingType::real:
            changed = real_widget(context, descriptor, value);
            break;
        case SettingType::path:
        case SettingType::text:
            changed = path_widget(context, descriptor, value, hint);
            break;
        case SettingType::boolean:
            break;
        }
    }
    ImGui::EndDisabled();
    if (global_only_here)
    {
        // A disabled item needs AllowWhenDisabled for its tooltip.
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("GLOBAL ONLY: SWITCH THE SCOPE TO GLOBAL TO CHANGE IT");
        }
    }
    else if (settings_ui_reset_button(context, key))
    {
        changed = true;
    }
    ImGui::PopID();
    return changed;
}
