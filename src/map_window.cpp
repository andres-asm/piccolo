#include "map_window.h"

#include <SDL3/SDL.h>

#include <cstdio>

#include "imgui.h"
#include "input_sdl.h"
#include "piccolo_text.h"
#include "retro_ui.h"

constexpr uint64_t OFFER_MS = 30000;   // NOT NOW by itself after this long

static const Str APPEARANCE_NAMES[] = {Str::map_generic, Str::map_xbox, Str::map_sony, Str::map_nintendo};

// The face, shoulder, trigger, stick click and menu button names of each
// appearance, in InputMapControl order up to the sticks (nullptr = from the
// strings: GENERIC's positions, and the d-pad / sticks of all of them).
static const char* const LABELS[4][INPUT_MAP_CONTROL_COUNT] = {
    // GENERIC
    {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "L", "R", "SELECT", "START", "L2", "R2", "L3",
     "R3", "HOME", nullptr, nullptr, nullptr, nullptr},
    // XBOX
    {"A", "B", "X", "Y", nullptr, nullptr, nullptr, nullptr, "LB", "RB", "VIEW", "MENU", "LT", "RT", "LS", "RS", "XBOX",
     nullptr, nullptr, nullptr, nullptr},
    // PLAYSTATION
    {"CROSS", "CIRCLE", "SQUARE", "TRIANGLE", nullptr, nullptr, nullptr, nullptr, "L1", "R1", "SHARE", "OPTIONS", "L2",
     "R2", "L3", "R3", "PS", nullptr, nullptr, nullptr, nullptr},
    // NINTENDO: A on the right, B at the bottom
    {"B", "A", "Y", "X", nullptr, nullptr, nullptr, nullptr, "L", "R", "-", "+", "ZL", "ZR", "LS", "RS", "HOME", nullptr,
     nullptr, nullptr, nullptr},
};

static const Str GENERIC_LABELS[INPUT_MAP_CONTROL_COUNT] = {
    Str::map_bottom, Str::map_right, Str::map_left, Str::map_top, Str::map_dpad_up, Str::map_dpad_down,
    Str::map_dpad_left, Str::map_dpad_right, Str::map_title, Str::map_title, Str::map_title, Str::map_title,
    Str::map_title, Str::map_title, Str::map_title, Str::map_title, Str::map_title, Str::map_left_x, Str::map_left_y,
    Str::map_right_x, Str::map_right_y,
};

static const char* control_label(const MapWindow* window, const Localization* strings, InputMapControl control)
{
    uint32_t index = static_cast<uint32_t>(control);
    const char* label = LABELS[static_cast<int>(window->appearance)][index];
    return label != nullptr ? label : piccolo_text(strings, GENERIC_LABELS[index]);
}

static bool is_stick(InputMapControl control)
{
    return static_cast<uint32_t>(control) >= static_cast<uint32_t>(InputMapControl::left_x);
}

void map_window_offer(MapWindow* window, uint32_t joystick_id, bool needed, uint64_t now_ms)
{
    map_window_open(window, joystick_id);
    window->stage = MapStage::offer;
    window->needed = needed;
    window->offer_started_ms = now_ms;
}

void map_window_open(MapWindow* window, uint32_t joystick_id)
{
    window->stage = MapStage::appearance;
    window->joystick_id = joystick_id;
    window->name = input_sdl_joystick_name(joystick_id);
    window->guid = input_sdl_joystick_guid(joystick_id);
    window->needed = !SDL_IsGamepad(static_cast<SDL_JoystickID>(joystick_id));
    // Preselected from what SDL knows of it.
    switch (SDL_GetGamepadTypeForID(static_cast<SDL_JoystickID>(joystick_id)))
    {
    case SDL_GAMEPAD_TYPE_XBOX360:
    case SDL_GAMEPAD_TYPE_XBOXONE:
        window->appearance = MapAppearance::xbox;
        break;
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5:
        window->appearance = MapAppearance::sony;
        break;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        window->appearance = MapAppearance::nintendo;
        break;
    default:
        window->appearance = MapAppearance::generic;
        break;
    }
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

static void draw_pad(InputMapControl lit, float unit)
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
        static const char* const IDS[] = {"generic", "xbox", "sony", "nintendo"};
        input_profile_for(context->input, window->guid)->appearance = IDS[static_cast<int>(window->appearance)];
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
    for (int a = 0; a < static_cast<int>(MapAppearance::count); a++)
    {
        ImGui::PushID(a);
        if (ImGui::Selectable(piccolo_text(strings, APPEARANCE_NAMES[a]), static_cast<int>(window->appearance) == a))
        {
            window->appearance = static_cast<MapAppearance>(a);
        }
        ImGui::PopID();
    }
    ImGui::Spacing();
    draw_pad(InputMapControl::count, block * 1.5f);
    if (retro_ui_button(piccolo_text(strings, Str::map_begin)))
    {
        if (input_sdl_mapper_begin(&window->mapper, window->joystick_id, context->now_ms))
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
    draw_pad(control, block * 1.5f);
    char text[300] = {};
    const char* label = control_label(window, strings, control);
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
    ImGui::SetNextWindowFocus();   // a modal: on top of the others
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
