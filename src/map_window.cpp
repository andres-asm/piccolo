#include "map_window.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <vector>

#include "imgui.h"
#include "input_sdl.h"
#include "piccolo_text.h"
#include "retro_ui.h"

constexpr uint64_t OFFER_MS = 30000;   // NOT NOW by itself after this long

// The names of the controls a layout doesn't name: the d-pad and the
// sticks (and any button a layout leaves without a label).
static const Str DEFAULT_LABELS[INPUT_MAP_CONTROL_COUNT] = {
    Str::map_bottom, Str::map_right, Str::map_left, Str::map_top, Str::map_dpad_up, Str::map_dpad_down,
    Str::map_dpad_left, Str::map_dpad_right, Str::map_title, Str::map_title, Str::map_title, Str::map_title,
    Str::map_title, Str::map_title, Str::map_title, Str::map_title, Str::map_title, Str::map_left_x, Str::map_left_y,
    Str::map_right_x, Str::map_right_y,
};

static const InputLayout* picked_layout(const MapWindow* window, const MapWindowContext* context)
{
    const InputLayout* layout = input_layouts_find(context->layouts, window->layout.c_str());
    return layout != nullptr ? layout : input_layouts_find(context->layouts, "Generic/generic");
}

static const char* control_label(const InputLayout* layout, const Localization* strings, InputMapControl control)
{
    uint32_t index = static_cast<uint32_t>(control);
    if (layout != nullptr && !layout->labels[index].empty())
    {
        return layout->labels[index].c_str();
    }
    return piccolo_text(strings, DEFAULT_LABELS[index]);
}

static bool is_stick(InputMapControl control)
{
    return static_cast<uint32_t>(control) >= static_cast<uint32_t>(InputMapControl::left_x);
}

void map_window_offer(MapWindow* window, uint32_t joystick_id, bool needed, const InputLayouts* layouts, uint64_t now_ms)
{
    map_window_open(window, joystick_id, layouts);
    window->stage = MapStage::offer;
    window->needed = needed;
    window->offer_started_ms = now_ms;
}

void map_window_open(MapWindow* window, uint32_t joystick_id, const InputLayouts* layouts)
{
    window->stage = MapStage::appearance;
    window->take_focus = true;
    window->joystick_id = joystick_id;
    window->name = input_sdl_joystick_name(joystick_id);
    window->guid = input_sdl_joystick_guid(joystick_id);
    window->needed = !SDL_IsGamepad(static_cast<SDL_JoystickID>(joystick_id));
    // Preselected from what SDL knows of it ("ps5", "xboxone"...).
    const char* type = SDL_GetGamepadStringForType(SDL_GetGamepadTypeForID(static_cast<SDL_JoystickID>(joystick_id)));
    const InputLayout* layout = layouts != nullptr ? input_layouts_for_type(layouts, type) : nullptr;
    window->layout = layout != nullptr ? layout->id : std::string("Generic/generic");
}

void map_window_close(MapWindow* window, InputSystem* input)
{
    input_sdl_mapper_cancel(&window->mapper);
    window->stage = MapStage::closed;
    input->suspended = false;
}

bool map_window_open_now(const MapWindow* window)
{
    return window->stage != MapStage::closed;
}

// ---------------------------------------------------------------------------
// The drawn pad
// ---------------------------------------------------------------------------

struct PadPart
{
    InputMapControl control;
    float           x0, y0, x1, y1;   // in a 100 x 46 box
};

static const PadPart PARTS[] = {
    {InputMapControl::left_trigger, 14, 0, 28, 4},
    {InputMapControl::right_trigger, 72, 0, 86, 4},
    {InputMapControl::left_shoulder, 12, 5, 30, 9},
    {InputMapControl::right_shoulder, 70, 5, 88, 9},
    {InputMapControl::dpad_up, 19, 14, 25, 19},
    {InputMapControl::dpad_down, 19, 25, 25, 30},
    {InputMapControl::dpad_left, 13, 19, 19, 25},
    {InputMapControl::dpad_right, 25, 19, 31, 25},
    {InputMapControl::north, 75, 13, 81, 19},
    {InputMapControl::south, 75, 25, 81, 31},
    {InputMapControl::west, 68, 19, 74, 25},
    {InputMapControl::east, 82, 19, 88, 25},
    {InputMapControl::back, 40, 16, 46, 19},
    {InputMapControl::start, 54, 16, 60, 19},
    {InputMapControl::guide, 47, 22, 53, 26},
    {InputMapControl::left_stick, 32, 30, 40, 38},
    {InputMapControl::right_stick, 60, 30, 68, 38},
};
constexpr size_t PART_COUNT = sizeof(PARTS) / sizeof(PARTS[0]);

static void draw_pad(const InputLayout* layout, InputMapControl lit, float unit)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    origin.x += static_cast<float>(static_cast<int>((width - 100.0f * unit) * 0.5f));
    ImU32 body = ImGui::GetColorU32(ImGuiCol_Button);
    ImU32 part = ImGui::GetColorU32(ImGuiCol_FrameBg);
    ImU32 light = retro_ui_attention_color();
    // The body, and the d-pad's middle.
    draw_list->AddRectFilled(ImVec2(origin.x + 8.0f * unit, origin.y + 10.0f * unit),
                             ImVec2(origin.x + 92.0f * unit, origin.y + 44.0f * unit), body);
    draw_list->AddRectFilled(ImVec2(origin.x + 19.0f * unit, origin.y + 19.0f * unit),
                             ImVec2(origin.x + 25.0f * unit, origin.y + 25.0f * unit), part);
    for (size_t p = 0; p < PART_COUNT; p++)
    {
        // Only what this kind of pad has (a stick: its click or its axes).
        InputMapControl control = PARTS[p].control;
        bool present = layout == nullptr || input_layout_has(layout, control) ||
                       (control == InputMapControl::left_stick && input_layout_has(layout, InputMapControl::left_x)) ||
                       (control == InputMapControl::right_stick && input_layout_has(layout, InputMapControl::right_x));
        if (!present)
        {
            continue;
        }
        // A stick lights for its click and for pushing it.
        bool on = PARTS[p].control == lit ||
                  (PARTS[p].control == InputMapControl::left_stick && (lit == InputMapControl::left_x || lit == InputMapControl::left_y)) ||
                  (PARTS[p].control == InputMapControl::right_stick && (lit == InputMapControl::right_x || lit == InputMapControl::right_y));
        draw_list->AddRectFilled(ImVec2(origin.x + PARTS[p].x0 * unit, origin.y + PARTS[p].y0 * unit),
                                 ImVec2(origin.x + PARTS[p].x1 * unit, origin.y + PARTS[p].y1 * unit), on ? light : part);
    }
    // Which way to push a stick: a mark on that side.
    if (is_stick(lit))
    {
        bool left = lit == InputMapControl::left_x || lit == InputMapControl::left_y;
        bool x = lit == InputMapControl::left_x || lit == InputMapControl::right_x;
        float cx = left ? 36.0f : 64.0f;
        float mx = x ? cx + 5.0f : cx - 1.0f;
        float my = x ? 33.0f : 39.0f;
        draw_list->AddRectFilled(ImVec2(origin.x + mx * unit, origin.y + my * unit),
                                 ImVec2(origin.x + (mx + 2.0f) * unit, origin.y + (my + 2.0f) * unit), light);
    }
    ImGui::Dummy(ImVec2(width, 46.0f * unit));
}

// ---------------------------------------------------------------------------
// The window
// ---------------------------------------------------------------------------

static void finish(MapWindow* window, const MapWindowContext* context)
{
    std::string line;
    char text[300] = {};
    if (input_sdl_mapper_finish(&window->mapper, context->mappings_path, &line))
    {
        input_profile_for(context->input, window->guid)->appearance = window->layout;
        window->profiles_dirty = true;
        std::snprintf(text, sizeof(text), piccolo_text(context->strings, Str::map_done), window->name.c_str());
    }
    else
    {
        std::snprintf(text, sizeof(text), piccolo_text(context->strings, Str::map_failed), window->name.c_str());
    }
    window->result = text;
    map_window_close(window, context->input);
}

static void not_now(MapWindow* window, const MapWindowContext* context)
{
    InputProfile* profile = input_profile_for(context->input, window->guid);
    if (profile->appearance.empty())
    {
        profile->appearance = "none";   // asked: not again
        window->profiles_dirty = true;
    }
    map_window_close(window, context->input);
}

static void draw_offer(MapWindow* window, const MapWindowContext* context)
{
    const Localization* strings = context->strings;
    char text[300] = {};
    std::snprintf(text, sizeof(text), piccolo_text(strings, Str::map_new), window->name.c_str());
    retro_ui_separator_text(text);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(piccolo_text(strings, window->needed ? Str::map_offer_needed : Str::map_offer_text));
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    if (retro_ui_button(piccolo_text(strings, Str::map_map)))
    {
        window->stage = MapStage::appearance;
        return;
    }
    ImGui::SameLine();
    uint64_t elapsed = context->now_ms - window->offer_started_ms;
    int left = static_cast<int>((OFFER_MS - (elapsed < OFFER_MS ? elapsed : OFFER_MS) + 999) / 1000);
    std::snprintf(text, sizeof(text), piccolo_text(strings, Str::map_not_now), left);
    if (retro_ui_button(text) || elapsed >= OFFER_MS)
    {
        not_now(window, context);
    }
}

static void draw_appearance(MapWindow* window, const MapWindowContext* context, float block)
{
    const Localization* strings = context->strings;
    retro_ui_separator_text(window->name.c_str());
    ImGui::TextUnformatted(piccolo_text(strings, Str::map_appearance));
    // MAKER, then MODEL among that maker's layouts.
    const InputLayout* layout = picked_layout(window, context);
    std::string maker = layout != nullptr ? layout->maker : std::string();
    std::vector<std::string> makers;
    input_layouts_makers(context->layouts, &makers);
    float label_width = 60.0f * block;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(piccolo_text(strings, Str::map_maker));
    ImGui::SameLine(label_width);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (retro_ui_begin_combo("##maker", maker.c_str()))
    {
        for (size_t m = 0; m < makers.size(); m++)
        {
            ImGui::PushID(static_cast<int>(m));
            if (ImGui::Selectable(makers[m].c_str(), makers[m] == maker))
            {
                // The maker's first model.
                for (size_t l = 0; l < context->layouts->layouts.size(); l++)
                {
                    if (context->layouts->layouts[l].maker == makers[m])
                    {
                        window->layout = context->layouts->layouts[l].id;
                        break;
                    }
                }
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    layout = picked_layout(window, context);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(piccolo_text(strings, Str::map_model));
    ImGui::SameLine(label_width);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (retro_ui_begin_combo("##model", layout != nullptr ? layout->name.c_str() : ""))
    {
        for (size_t l = 0; layout != nullptr && l < context->layouts->layouts.size(); l++)
        {
            const InputLayout* candidate = &context->layouts->layouts[l];
            if (candidate->maker != layout->maker)
            {
                continue;
            }
            ImGui::PushID(static_cast<int>(l));
            if (ImGui::Selectable(candidate->name.c_str(), candidate->id == window->layout))
            {
                window->layout = candidate->id;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    layout = picked_layout(window, context);
    ImGui::Spacing();
    draw_pad(layout, InputMapControl::count, block * 1.5f);
    bool wanted[INPUT_MAP_CONTROL_COUNT] = {};
    for (uint32_t c = 0; c < INPUT_MAP_CONTROL_COUNT; c++)
    {
        wanted[c] = layout == nullptr || input_layout_has(layout, static_cast<InputMapControl>(c));
    }
    if (retro_ui_button(piccolo_text(strings, Str::map_begin)))
    {
        if (input_sdl_mapper_begin(&window->mapper, window->joystick_id, wanted, context->now_ms))
        {
            window->stage = MapStage::mapping;
            context->input->suspended = true;   // its presses are the mapping's alone
        }
    }
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::map_cancel)))
    {
        map_window_close(window, context->input);
    }
}

static void draw_mapping(MapWindow* window, const MapWindowContext* context, float block)
{
    const Localization* strings = context->strings;
    if (input_sdl_mapper_update(&window->mapper, context->now_ms))
    {
        finish(window, context);
        return;
    }
    if (!window->mapper.active)
    {
        map_window_close(window, context->input);   // went away
        return;
    }
    InputMapControl control = static_cast<InputMapControl>(window->mapper.step);
    retro_ui_separator_text(window->name.c_str());
    const InputLayout* layout = picked_layout(window, context);
    draw_pad(layout, control, block * 1.5f);
    char text[300] = {};
    const char* label = control_label(layout, strings, control);
    if (is_stick(control))
    {
        std::snprintf(text, sizeof(text), "%s", label);
    }
    else
    {
        std::snprintf(text, sizeof(text), piccolo_text(strings, Str::map_press), label);
    }
    retro_ui_push_section_text();
    ImGui::TextUnformatted(text);
    retro_ui_pop_section_text();
    uint32_t left = input_sdl_mapper_seconds_left(&window->mapper, context->now_ms);
    if (left > 0)
    {
        std::snprintf(text, sizeof(text), piccolo_text(strings, Str::map_skip_in), static_cast<int>(left));
        ImGui::TextDisabled("%s", text);
    }
    else
    {
        ImGui::TextUnformatted("");
    }
    ImGui::BeginDisabled(window->mapper.step == 0);
    if (retro_ui_button(piccolo_text(strings, Str::map_back)))
    {
        input_sdl_mapper_back(&window->mapper, context->now_ms);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::map_skip)))
    {
        input_sdl_mapper_skip(&window->mapper, context->now_ms);
    }
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::map_cancel)))
    {
        map_window_close(window, context->input);
    }
}

void map_window_draw(MapWindow* window, const MapWindowContext* context, ImFont* pixel_font, int32_t scale)
{
    if (window->stage == MapStage::closed)
    {
        return;
    }
    float block = static_cast<float>(scale);
    const ImGuiIO& io = ImGui::GetIO();
    retro_ui_push_theme(pixel_font, scale);
    ImVec2 size(240.0f * block, 170.0f * block);
    size.x = size.x < io.DisplaySize.x - 8.0f * block ? size.x : io.DisplaySize.x - 8.0f * block;
    size.y = size.y < io.DisplaySize.y - 24.0f * block ? size.y : io.DisplaySize.y - 24.0f * block;
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (window->take_focus)
    {
        ImGui::SetNextWindowFocus();   // on top of the others when it opens
        window->take_focus = false;
    }
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    char title[200] = {};
    std::snprintf(title, sizeof(title), "%s###map_controller", piccolo_text(context->strings, Str::map_title));
    if (ImGui::Begin(title, nullptr, flags))
    {
        retro_ui_begin_window(scale);
        switch (window->stage)
        {
        case MapStage::offer:
            draw_offer(window, context);
            break;
        case MapStage::appearance:
            draw_appearance(window, context, block);
            break;
        case MapStage::mapping:
            draw_mapping(window, context, block);
            break;
        case MapStage::closed:
            break;
        }
        retro_ui_end_window();
    }
    ImGui::End();
    retro_ui_pop_theme();
}
