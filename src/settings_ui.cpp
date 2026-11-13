#include "settings_ui.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "imgui.h"

#include "libretro_host.h"   // LIBRETRO_PATH_MAX
#include "piccolo_text.h"
#include "retro_ui.h"

// Marker colors: our own, distinct from the theme's blues (0xAARRGGBB).
constexpr uint32_t MARKER_CORE = 0xFFF0C040u;   // amber
constexpr uint32_t MARKER_GAME = 0xFF60E080u;   // green

static ImVec4 color_of(uint32_t argb)
{
    return ImVec4(static_cast<float>((argb >> 16) & 0xFFu) / 255.0f, static_cast<float>((argb >> 8) & 0xFFu) / 255.0f,
                  static_cast<float>(argb & 0xFFu) / 255.0f, static_cast<float>(argb >> 24) / 255.0f);
}

const char* settings_ui_scope_name(const Localization* strings, SettingLayer scope)
{
    switch (scope)
    {
    case SettingLayer::defaults:
    case SettingLayer::global:
        return piccolo_text(strings, Str::scope_global);
    case SettingLayer::core:
        return piccolo_text(strings, Str::scope_core);
    case SettingLayer::game:
        return piccolo_text(strings, Str::scope_game);
    }
    return piccolo_text(strings, Str::scope_global);
}

float settings_ui_label_width(const Settings* settings, const Localization* strings)
{
    float widest = 0.0f;
    for (uint32_t i = 0; i < settings->descriptor_count; i++)
    {
        const SettingDescriptor* descriptor = &settings->descriptors[i];
        if (descriptor->type == SettingType::boolean || descriptor->label == SETTING_NO_TEXT)
        {
            continue;
        }
        float width = ImGui::CalcTextSize(piccolo_text_id(strings, descriptor->label)).x;
        widest = width > widest ? width : widest;
    }
    return widest + ImGui::GetStyle().ItemSpacing.x * 2.0f;
}

void settings_ui_marker(const SettingsUiContext* context, const char* key)
{
    SettingLayer source = SettingLayer::defaults;
    settings_get(context->settings, key, &source);
    ImVec2 size = ImGui::CalcTextSize("C");
    if (source != SettingLayer::core && source != SettingLayer::game)
    {
        ImGui::Dummy(size);
        return;
    }
    bool core = source == SettingLayer::core;
    ImGui::TextColored(color_of(core ? MARKER_CORE : MARKER_GAME), "%s", core ? "C" : "G");
    ImGui::SetItemTooltip("%s", piccolo_text(context->strings, core ? Str::marker_core_help : Str::marker_game_help));
}

bool settings_ui_reset_button(const SettingsUiContext* context, const char* key)
{
    if (settings_get_layer(context->settings, context->scope, key) == nullptr)
    {
        return false;
    }
    const char* reset = piccolo_text(context->strings, Str::reset);
    float width = ImGui::CalcTextSize(reset).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::SameLine(ImGui::GetContentRegionMax().x - width);
    ImGui::PushID("reset");
    bool clicked = ImGui::Button(reset);
    ImGui::PopID();
    Str help = context->scope == SettingLayer::global ? Str::reset_help_global : Str::reset_help_override;
    ImGui::SetItemTooltip("%s", piccolo_text(context->strings, help));
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
    ImGui::TextUnformatted(piccolo_text_id(context->strings, descriptor->label));
    if (descriptor->help != SETTING_NO_TEXT)
    {
        ImGui::SetItemTooltip("%s", piccolo_text_id(context->strings, descriptor->help));
    }
    ImGui::SameLine(start + context->label_width);
}

// Room left for the widget, keeping space for the reset button.
static float widget_width(const SettingsUiContext* context)
{
    float reset = ImGui::CalcTextSize(piccolo_text(context->strings, Str::reset)).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    return ImGui::GetContentRegionAvail().x - reset - ImGui::GetStyle().ItemSpacing.x;
}

// An enum value's label in the current language, or its key ("4:3") when it has none.
static const char* value_label(const SettingsUiContext* context, const SettingEnumValue* value)
{
    return value->label == SETTING_NO_TEXT ? value->key : piccolo_text_id(context->strings, value->label);
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
    ImGui::SetNextItemWidth(widget_width(context));
    if (retro_ui_begin_combo("##value", value_label(context, &descriptor->values[current])))
    {
        for (uint32_t i = 0; i < descriptor->value_count; i++)
        {
            ImGui::PushID(static_cast<int>(i));
            bool picked = ImGui::Selectable(value_label(context, &descriptor->values[i]), i == current);
            ImGui::PopID();
            if (picked && i != current)
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
    ImGui::SetNextItemWidth(widget_width(context));
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
    ImGui::SetNextItemWidth(widget_width(context));
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
    ImGui::SetNextItemWidth(widget_width(context));
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
    settings_ui_marker(context, key);
    ImGui::BeginDisabled(global_only_here);
    if (descriptor->type == SettingType::boolean)
    {
        ImGui::SameLine();
        bool checked = std::strcmp(value, "true") == 0;
        // "###" keeps the widget's ID the same whatever the language.
        char label[256] = {};
        std::snprintf(label, sizeof(label), "%s###check", piccolo_text_id(context->strings, descriptor->label));
        if (retro_ui_checkbox(label, &checked))
        {
            changed = settings_set_bool(context->settings, context->scope, key, checked);
        }
        if (descriptor->help != SETTING_NO_TEXT)
        {
            ImGui::SetItemTooltip("%s", piccolo_text_id(context->strings, descriptor->help));
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
            ImGui::SetTooltip("%s", piccolo_text(context->strings, Str::global_only_help));
        }
    }
    else if (settings_ui_reset_button(context, key))
    {
        changed = true;
    }
    ImGui::PopID();
    return changed;
}
