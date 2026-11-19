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
    retro_ui_item_tooltip(piccolo_text(context->strings, core ? Str::marker_core_help : Str::marker_game_help));
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
    bool clicked = retro_ui_button(reset);
    ImGui::PopID();
    Str help = context->scope == SettingLayer::global ? Str::reset_help_global : Str::reset_help_override;
    retro_ui_item_tooltip(piccolo_text(context->strings, help));
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
        retro_ui_item_tooltip(piccolo_text_id(context->strings, descriptor->help));
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
    retro_ui_push_field_text();
    bool edited = ImGui::InputInt("##value", &number, descriptor->step > 0 ? descriptor->step : 1, 8);
    retro_ui_pop_field_text();
    if (!edited)
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

// The color as it resolves at the edited scope (0xFFRRGGBB).
static uint32_t settings_get_color_at(const SettingsUiContext* context, const char* key)
{
    uint32_t argb = 0xFF000000u;
    const char* value = settings_get_at(context->settings, context->scope, key, nullptr);
    if (value != nullptr)
    {
        settings_color_from_text(value, &argb);
    }
    return argb;
}

// A swatch (click: ImGui's color picker) and the value as #RRGGBB.
static bool color_widget(const SettingsUiContext* context, const SettingDescriptor* descriptor)
{
    uint32_t argb = settings_get_color_at(context, descriptor->key);
    float rgb[3] = {static_cast<float>((argb >> 16) & 0xFFu) / 255.0f, static_cast<float>((argb >> 8) & 0xFFu) / 255.0f,
                    static_cast<float>(argb & 0xFFu) / 255.0f};
    bool changed = false;
    if (ImGui::ColorEdit3("##value", rgb, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
                                             ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoAlpha))
    {
        uint32_t picked = 0xFF000000u;
        for (int i = 0; i < 3; i++)
        {
            float channel = rgb[i] < 0.0f ? 0.0f : (rgb[i] > 1.0f ? 1.0f : rgb[i]);
            picked |= static_cast<uint32_t>(channel * 255.0f + 0.5f) << (16 - 8 * i);
        }
        changed = settings_set_color(context->settings, context->scope, descriptor->key, picked);
    }
    retro_ui_raised_item_edge();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("#%06X", static_cast<unsigned>(argb & 0xFFFFFFu));
    return changed;
}

static bool path_widget(const SettingsUiContext* context, const SettingDescriptor* descriptor, const char* value,
                        const char* hint)
{
    // ImGui keeps its own copy of the text while the field is being edited;
    // this buffer only seeds it. The value is set when editing ends.
    char buffer[LIBRETRO_PATH_MAX] = {};
    std::snprintf(buffer, sizeof(buffer), "%s", value);
    ImGui::SetNextItemWidth(widget_width(context));
    retro_ui_push_field_text();
    ImGui::InputTextWithHint("##value", hint != nullptr ? hint : "", buffer, sizeof(buffer));
    retro_ui_pop_field_text();
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
    // Only when it is: ImGui counts BeginDisabled(false) too, and its color
    // swatches then draw with a saved opacity that may be 0 (invisible).
    if (global_only_here)
    {
        ImGui::BeginDisabled();
    }
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
            retro_ui_item_tooltip(piccolo_text_id(context->strings, descriptor->help));
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
        case SettingType::color:
            changed = color_widget(context, descriptor);
            break;
        case SettingType::boolean:
            break;
        }
    }
    if (global_only_here)
    {
        ImGui::EndDisabled();
    }
    if (global_only_here)
    {
        // A disabled item needs AllowWhenDisabled for its tooltip.
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            retro_ui_tooltip(piccolo_text(context->strings, Str::global_only_help));
        }
    }
    else if (settings_ui_reset_button(context, key))
    {
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

bool settings_ui_folder_field(const SettingsUiContext* context, const char* value, const char* hint, float reserved)
{
    // Read-only: it looks like any other field (and a long path can be
    // scrolled), but a click opens CHOOSE FOLDER, as "..." does for whoever
    // doesn't think of clicking the field.
    const char* dots = "...";
    float dots_width = ImGui::CalcTextSize(dots).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    char buffer[LIBRETRO_PATH_MAX] = {};
    std::snprintf(buffer, sizeof(buffer), "%s", value);
    // Up to the right edge (Andrés: no empty column after it), less what
    // the caller's button needs.
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - reserved - dots_width - spacing);
    retro_ui_push_field_text();
    ImGui::InputTextWithHint("##value", hint != nullptr ? hint : "", buffer, sizeof(buffer),
                             ImGuiInputTextFlags_ReadOnly);
    retro_ui_pop_field_text();
    bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    retro_ui_item_tooltip(piccolo_text(context->strings, Str::game_folder_choose_help));
    ImGui::SameLine();
    ImGui::PushID("choose");
    if (retro_ui_button(dots))
    {
        clicked = true;
    }
    ImGui::PopID();
    retro_ui_item_tooltip(piccolo_text(context->strings, Str::game_folder_choose_help));
    return clicked;
}

bool settings_ui_folder_row(const SettingsUiContext* context, const char* key, const char* hint)
{
    const SettingDescriptor* descriptor = settings_find(context->settings, key);
    if (descriptor == nullptr)
    {
        return false;
    }
    bool global_only_here = !descriptor->overridable && context->scope != SettingLayer::global;
    const char* value = settings_get_at(context->settings, context->scope, key, nullptr);
    bool clicked = false;

    ImGui::PushID(key);
    settings_ui_marker(context, key);
    if (global_only_here)
    {
        ImGui::BeginDisabled();
    }
    label_column(context, descriptor);
    // RESET only when this scope sets the folder: then its room is kept.
    float reserved = 0.0f;
    if (!global_only_here && settings_get_layer(context->settings, context->scope, key) != nullptr)
    {
        reserved = ImGui::CalcTextSize(piccolo_text(context->strings, Str::reset)).x +
                   ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x;
    }
    clicked = settings_ui_folder_field(context, value, hint, reserved) && !global_only_here;
    if (global_only_here)
    {
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            retro_ui_tooltip(piccolo_text(context->strings, Str::global_only_help));
        }
    }
    else
    {
        settings_ui_reset_button(context, key);
    }
    ImGui::PopID();
    return clicked;
}

bool settings_ui_text_choice_row(const SettingsUiContext* context, const char* key, const char* const* choices,
                                 uint32_t choice_count, const char* empty_label, const char* missing_label)
{
    const SettingDescriptor* descriptor = settings_find(context->settings, key);
    if (descriptor == nullptr)
    {
        return false;
    }
    bool global_only_here = !descriptor->overridable && context->scope != SettingLayer::global;
    const char* value = settings_get_at(context->settings, context->scope, key, nullptr);
    bool listed = value[0] == '\0';
    for (uint32_t i = 0; i < choice_count && !listed; i++)
    {
        listed = std::strcmp(choices[i], value) == 0;
    }
    char preview[512] = {};
    if (value[0] == '\0')
    {
        std::snprintf(preview, sizeof(preview), "%s", empty_label);
    }
    else if (listed)
    {
        std::snprintf(preview, sizeof(preview), "%s", value);
    }
    else
    {
        std::snprintf(preview, sizeof(preview), "%s (%s)", value, missing_label);
    }

    bool changed = false;
    ImGui::PushID(key);
    settings_ui_marker(context, key);
    ImGui::BeginDisabled(global_only_here);
    label_column(context, descriptor);
    ImGui::SetNextItemWidth(widget_width(context));
    if (retro_ui_begin_combo("##value", preview))
    {
        if (ImGui::Selectable(empty_label, value[0] == '\0') && value[0] != '\0')
        {
            changed = settings_reset(context->settings, context->scope, key);   // "" = the default
        }
        for (uint32_t i = 0; i < choice_count; i++)
        {
            ImGui::PushID(static_cast<int>(i));
            bool current = std::strcmp(choices[i], value) == 0;
            bool picked = ImGui::Selectable(choices[i], current);
            ImGui::PopID();
            if (picked && !current)
            {
                changed = settings_set(context->settings, context->scope, key, choices[i]);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if (global_only_here)
    {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            retro_ui_tooltip(piccolo_text(context->strings, Str::global_only_help));
        }
    }
    else if (settings_ui_reset_button(context, key))
    {
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

void settings_ui_scope(Settings* settings, const Localization* strings, const char* core_name, const char* game_name,
                       SettingLayer* scope, bool* choose_scope)
{
    bool has_core = core_name[0] != '\0';
    bool has_game = has_core && game_name[0] != '\0';
    if (*choose_scope)
    {
        // The most specific layer already in use, so changes keep going there.
        *choose_scope = false;
        *scope = SettingLayer::global;
        if (has_game && settings_layer_has_values(settings, SettingLayer::game))
        {
            *scope = SettingLayer::game;
        }
        else if (has_core && settings_layer_has_values(settings, SettingLayer::core))
        {
            *scope = SettingLayer::core;
        }
    }
    if ((*scope == SettingLayer::core && !has_core) || (*scope == SettingLayer::game && !has_game))
    {
        *scope = SettingLayer::global;   // the game stopped
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
                  core_name);
    std::snprintf(game_label, sizeof(game_label), "%s (%s)###game", piccolo_text(strings, Str::scope_game),
                  game_name);
    char global_label[200] = {};
    std::snprintf(global_label, sizeof(global_label), "%s###global", piccolo_text(strings, Str::scope_global));
    // The combo shows the label without its "###" part.
    const char* preview = *scope == SettingLayer::core   ? core_label
                          : *scope == SettingLayer::game ? game_label
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
        if (ImGui::Selectable(global_label, *scope == SettingLayer::global))
        {
            *scope = SettingLayer::global;
        }
        if (ImGui::Selectable(core_label, *scope == SettingLayer::core))
        {
            *scope = SettingLayer::core;
        }
        if (has_game && ImGui::Selectable(game_label, *scope == SettingLayer::game))
        {
            *scope = SettingLayer::game;
        }
        ImGui::EndCombo();
    }
}

