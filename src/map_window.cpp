#include "map_window.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <vector>

#include "imgui.h"
#include "input_sdl.h"
#include "pad_picture.h"
#include "piccolo_text.h"
#include "retro_ui.h"

constexpr uint64_t OFFER_MS = 15000;   // NOT NOW by itself after this long
constexpr uint64_t INTRO_MS = 10000;
constexpr uint64_t APPEARANCE_MS = 15000;   // SELECT CONTROLLER TYPE: START by itself after this long untouched   // a step's page STARTs by itself after this long

// The names of the controls a layout doesn't name: the d-pad and the
// sticks (and any button a layout leaves without a label).
static const Str DEFAULT_LABELS[INPUT_MAP_CONTROL_COUNT] = {
    Str::map_bottom, Str::map_right, Str::map_left, Str::map_top, Str::map_dpad_up, Str::map_dpad_down,
    Str::map_dpad_left, Str::map_dpad_right, Str::map_title, Str::map_title, Str::map_title, Str::map_title,
    Str::map_title, Str::map_title, Str::map_title, Str::map_title, Str::map_title, Str::map_left_x, Str::map_left_y,
    Str::map_right_x, Str::map_right_y,
};

// THE INTERFACE: the actions each role step binds, in the order asked.
struct MapRole
{
    Str         label;
    InputAction action;
    InputAction also;       // the same button goes here too (count = nothing more)
    bool        optional;   // a pad may not have it (shoulders for the pages)
    bool        navigate;   // keeps the default's sources of the other kind (d-pad role: the stick stays)
};

static const MapRole ROLES[] = {
    {Str::map_role_up, InputAction::ui_up, InputAction::count, false, true},
    {Str::map_role_down, InputAction::ui_down, InputAction::count, false, true},
    {Str::map_role_left, InputAction::ui_left, InputAction::count, false, true},
    {Str::map_role_right, InputAction::ui_right, InputAction::count, false, true},
    {Str::map_role_page_left, InputAction::ui_page_left, InputAction::count, true, false},
    {Str::map_role_page_right, InputAction::ui_page_right, InputAction::count, true, false},
    {Str::map_role_accept, InputAction::ui_accept, InputAction::count, false, false},
    {Str::map_role_back, InputAction::ui_back, InputAction::count, false, false},
    // The main menu: opens it in the interface, and with HOTKEY ENABLE in a game.
    {Str::map_role_menu, InputAction::ui_open_menu, InputAction::hotkey_menu, false, false},
    {Str::map_role_modifier, InputAction::hotkey_enable, InputAction::count, false, false},
    {Str::map_role_quick_menu, InputAction::hotkey_quick_menu, InputAction::count, false, false},
};
constexpr uint32_t ROLE_COUNT = sizeof(ROLES) / sizeof(ROLES[0]);

static const Str MODE_NAMES[] = {Str::hotkey_mode_hold, Str::hotkey_mode_one, Str::hotkey_mode_stays};
static const InputHotkeyMode MODES[] = {InputHotkeyMode::hold, InputHotkeyMode::one_hotkey, InputHotkeyMode::stays_on};

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

void map_window_offer(MapWindow* window, uint32_t joystick_id, bool needed, const InputLayouts* layouts, uint64_t now_ms)
{
    map_window_open(window, joystick_id, layouts);
    window->stage = MapStage::offer;
    window->offered = true;
    window->needed = needed;
    window->offer_started_ms = now_ms;
}

void map_window_open(MapWindow* window, uint32_t joystick_id, const InputLayouts* layouts)
{
    window->stage = MapStage::appearance;
    window->offered = false;
    window->take_focus = true;
    window->joystick_id = joystick_id;
    window->name = input_sdl_joystick_name(joystick_id);
    window->guid = input_sdl_joystick_guid(joystick_id);
    window->needed = !SDL_IsGamepad(static_cast<SDL_JoystickID>(joystick_id));
    // Preselected from what SDL knows of it ("ps5", "xboxone"...).
    const char* type = SDL_GetGamepadStringForType(SDL_GetGamepadTypeForID(static_cast<SDL_JoystickID>(joystick_id)));
    const InputLayout* layout = layouts != nullptr ? input_layouts_for_type(layouts, type) : nullptr;
    window->layout = layout != nullptr ? layout->id : std::string("Generic/generic");
    window->hotkey_mode = InputHotkeyMode::hold;
}

void map_window_close(MapWindow* window, InputSystem* input)
{
    input_sdl_mapper_cancel(&window->mapper);
    input_sdl_press_close(&window->press);
    window->press_stage = MapStage::closed;
    window->drawn_stage = MapStage::closed;
    window->stage = MapStage::closed;
    input->suspended = false;
}

bool map_window_open_now(const MapWindow* window)
{
    return window->stage != MapStage::closed;
}

// ---------------------------------------------------------------------------
// The window
// ---------------------------------------------------------------------------

// The roles as changes to the profile's bindings (and to the controller's
// own, if it is in use: what is kept is taken from it).
static void apply_roles(MapWindow* window, const MapWindowContext* context)
{
    InputSystem* input = context->input;
    InputProfile* profile = input_profile_for(input, window->guid);
    profile->hotkey_mode = window->hotkey_mode;
    InputBindingSet defaults = {};
    input_default_gamepad_bindings(&defaults);
    for (uint32_t r = 0; r < ROLE_COUNT; r++)
    {
        InputSource source = input_sdl_mapper_role_source(&window->mapper, r);
        InputAction action = ROLES[r].action;
        InputBindingOverride change = {};
        change.action = action;
        uint32_t count = 0;
        if (source.type != InputSourceType::none)
        {
            change.sources[count++] = source;
        }
        else if (!ROLES[r].optional)
        {
            continue;   // skipped: the default stays
        }
        for (uint32_t s = 0; ROLES[r].navigate && s < INPUT_SOURCES_PER_ACTION; s++)
        {
            InputSource kept = defaults.sources[static_cast<uint32_t>(action)][s];
            bool other_kind = (kept.type == InputSourceType::button) != (source.type == InputSourceType::button);
            if (kept.type != InputSourceType::none && other_kind && count < INPUT_SOURCES_PER_ACTION)
            {
                change.sources[count++] = kept;
            }
        }
        InputAction targets[2] = {action, ROLES[r].also};
        for (uint32_t t = 0; t < 2 && targets[t] != InputAction::count; t++)
        {
            change.action = targets[t];
            bool found = false;
            for (size_t o = 0; o < profile->overrides.size() && !found; o++)
            {
                if (profile->overrides[o].action == targets[t])
                {
                    profile->overrides[o] = change;
                    found = true;
                }
            }
            if (!found)
            {
                profile->overrides.push_back(change);
            }
            for (size_t c = 0; c < input->controllers.size(); c++)
            {
                if (input_controller_profile_id(input, static_cast<uint32_t>(c)) != window->guid)
                {
                    continue;
                }
                input_unbind_all(&input->controllers[c].bindings, targets[t]);
                for (uint32_t s = 0; s < count; s++)
                {
                    input_bind(&input->controllers[c].bindings, targets[t], change.sources[s]);
                }
            }
        }
    }
    for (size_t c = 0; c < input->controllers.size(); c++)
    {
        if (input_controller_profile_id(input, static_cast<uint32_t>(c)) == window->guid)
        {
            input->controllers[c].hotkey_mode = window->hotkey_mode;
        }
    }
}

static void finish(MapWindow* window, const MapWindowContext* context)
{
    std::string line;
    char text[300] = {};
    if (input_sdl_mapper_finish(&window->mapper, &line))
    {
        apply_roles(window, context);
        InputProfile* profile = input_profile_for(context->input, window->guid);
        profile->appearance = window->layout;
        profile->sdl_mapping = line;
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

// The wizard's buttons: at the bottom of the window, always.
static void footer()
{
    float y = ImGui::GetWindowHeight() - ImGui::GetStyle().WindowPadding.y - ImGui::GetFrameHeight();
    if (ImGui::GetCursorPosY() < y)
    {
        ImGui::SetCursorPosY(y);
    }
}

static void wrapped(const char* text)
{
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
}

static void section(const char* text)
{
    retro_ui_push_section_text();
    ImGui::TextUnformatted(text);
    retro_ui_pop_section_text();
}

// Seconds left of a countdown of `total_ms` started at `started_ms`.
static int seconds_left(uint64_t now_ms, uint64_t started_ms, uint64_t total_ms)
{
    uint64_t elapsed = now_ms - started_ms;
    return static_cast<int>((total_ms - (elapsed < total_ms ? elapsed : total_ms) + 999) / 1000);
}

// A press of any button on the controller being mapped, for pages it can
// answer that way: one SDL doesn't know can't drive the interface. Each page
// starts from everything let go (the press that opened it doesn't count).
static bool raw_press(MapWindow* window)
{
    if (window->press_stage != window->stage)
    {
        window->press_stage = window->stage;
        input_sdl_press_wait_release(&window->press);
    }
    return input_sdl_press_poll(&window->press, window->joystick_id);
}

static void draw_offer(MapWindow* window, const MapWindowContext* context)
{
    const Localization* strings = context->strings;
    char text[300] = {};
    retro_ui_separator_text(window->name.c_str());
    wrapped(piccolo_text(strings, Str::map_offer_text));
    if (window->needed)
    {
        ImGui::Spacing();
        wrapped(piccolo_text(strings, Str::map_offer_needed));
    }
    ImGui::Spacing();
    wrapped(piccolo_text(strings, Str::map_any_button));
    footer();
    // Any button on the new controller is MAP, whether SDL knows it or not.
    bool pressed = raw_press(window);
    if (retro_ui_button(piccolo_text(strings, Str::map_map)) || pressed)
    {
        window->stage = MapStage::appearance;
        return;
    }
    ImGui::SameLine();
    std::snprintf(text, sizeof(text), piccolo_text(strings, Str::map_not_now),
                  seconds_left(context->now_ms, window->offer_started_ms, OFFER_MS));
    if (retro_ui_button(text) || context->now_ms - window->offer_started_ms >= OFFER_MS)
    {
        not_now(window, context);
    }
}

static void draw_appearance(MapWindow* window, const MapWindowContext* context, float block)
{
    const Localization* strings = context->strings;
    retro_ui_separator_text(window->name.c_str());
    ImGui::TextUnformatted(piccolo_text(strings, Str::map_appearance));
    // LABELS (the folder), then LAYOUT among that folder's layouts.
    const InputLayout* layout = picked_layout(window, context);
    std::string maker = layout != nullptr ? layout->maker : std::string();
    std::vector<std::string> makers;
    input_layouts_makers(context->layouts, &makers);
    float label_width = 80.0f * block;
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
                // The folder's first layout.
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
    // HOTKEY MODE: how its HOTKEY ENABLE (asked in STEP 1) turns hotkeys on.
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(piccolo_text(strings, Str::input_hotkey_mode));
    retro_ui_item_tooltip(piccolo_text(strings, Str::input_hotkey_mode_help));
    ImGui::SameLine(label_width);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (retro_ui_begin_combo("##hotkey_mode", piccolo_text(strings, MODE_NAMES[static_cast<int>(window->hotkey_mode)])))
    {
        for (int m = 0; m < 3; m++)
        {
            if (ImGui::Selectable(piccolo_text(strings, MODE_NAMES[m]), window->hotkey_mode == MODES[m]))
            {
                window->hotkey_mode = MODES[m];
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Spacing();
    pad_picture_draw(layout, InputMapControl::count, false, block * 1.5f);
    if (window->needed)
    {
        wrapped(piccolo_text(strings, Str::map_any_button));
    }
    footer();
    // Touching any list (or choosing in it) starts the countdown again.
    if (ImGui::IsAnyItemActive() || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
    {
        window->appearance_started_ms = context->now_ms;
    }
    char begin[100] = {};
    std::snprintf(begin, sizeof(begin), piccolo_text(strings, Str::map_start),
                  seconds_left(context->now_ms, window->appearance_started_ms, APPEARANCE_MS));
    bool pressed = window->needed && raw_press(window);
    bool timed_out = context->now_ms - window->appearance_started_ms >= APPEARANCE_MS;
    if (retro_ui_button(begin) || pressed || timed_out)
    {
        window->stage = MapStage::intro_buttons;
        window->intro_started_ms = context->now_ms;
    }
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::map_cancel)))
    {
        map_window_close(window, context->input);
    }
}

// STEP 1's page (CONTROLLER BUTTONS): START begins the mapper on its
// position steps (the router is suspended from then on).
static void draw_intro_buttons(MapWindow* window, const MapWindowContext* context, float block)
{
    const Localization* strings = context->strings;
    char text[200] = {};
    retro_ui_separator_text(window->name.c_str());
    section(piccolo_text(strings, Str::map_step_buttons));
    wrapped(piccolo_text(strings, Str::map_step_buttons_text));
    ImGui::Spacing();
    pad_picture_draw(picked_layout(window, context), InputMapControl::count, false, block * 1.5f);
    wrapped(piccolo_text(strings, Str::map_any_button));
    footer();
    std::snprintf(text, sizeof(text), piccolo_text(strings, Str::map_start),
                  seconds_left(context->now_ms, window->intro_started_ms, INTRO_MS));
    if (retro_ui_button(text) || raw_press(window) || context->now_ms - window->intro_started_ms >= INTRO_MS)
    {
        const InputLayout* layout = picked_layout(window, context);
        bool wanted[INPUT_MAP_CONTROL_COUNT] = {};
        for (uint32_t c = 0; c < INPUT_MAP_CONTROL_COUNT; c++)
        {
            wanted[c] = layout == nullptr || input_layout_has(layout, static_cast<InputMapControl>(c));
        }
        bool optional[ROLE_COUNT] = {};
        for (uint32_t r = 0; r < ROLE_COUNT; r++)
        {
            optional[r] = ROLES[r].optional;
        }
        if (input_sdl_mapper_begin(&window->mapper, window->joystick_id, ROLE_COUNT, optional, wanted, context->now_ms))
        {
            window->stage = MapStage::mapping;
            window->interface_intro_done = false;
            context->input->suspended = true;   // its presses are the mapping's alone
        }
        else
        {
            map_window_close(window, context->input);   // went away
        }
        return;
    }
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::map_back)))
    {
        window->stage = MapStage::appearance;
        return;
    }
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::map_cancel)))
    {
        map_window_close(window, context->input);
    }
}

// STEP 2's page (USER INTERFACE); the mapper waits on the first role.
static void draw_intro_interface(MapWindow* window, const MapWindowContext* context, float block)
{
    const Localization* strings = context->strings;
    char text[200] = {};
    retro_ui_separator_text(window->name.c_str());
    section(piccolo_text(strings, Str::map_step_interface));
    wrapped(piccolo_text(strings, Str::map_step_interface_text));
    ImGui::Spacing();
    pad_picture_draw(picked_layout(window, context), InputMapControl::count, false, block * 1.5f);
    wrapped(piccolo_text(strings, Str::map_any_button));   // the router is suspended: no controller drives the page
    footer();
    std::snprintf(text, sizeof(text), piccolo_text(strings, Str::map_start),
                  seconds_left(context->now_ms, window->intro_started_ms, INTRO_MS));
    if (retro_ui_button(text) || raw_press(window) || context->now_ms - window->intro_started_ms >= INTRO_MS)
    {
        window->stage = MapStage::mapping;
        window->interface_intro_done = true;
        input_sdl_mapper_restart_step(&window->mapper, context->now_ms);
        return;
    }
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::map_back)))
    {
        input_sdl_mapper_back(&window->mapper, context->now_ms);   // STEP 1's last button again
        window->stage = MapStage::mapping;
        return;
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
    int32_t role = input_sdl_mapper_role(&window->mapper);
    if (role >= 0 && !window->interface_intro_done)
    {
        window->stage = MapStage::intro_interface;   // STEP 1 is done
        window->intro_started_ms = context->now_ms;
        return;
    }
    if (role < 0)
    {
        window->interface_intro_done = false;   // back in STEP 1: STEP 2's page comes again
    }
    InputMapControl control = input_sdl_mapper_control(&window->mapper);
    char text[300] = {};
    retro_ui_separator_text(window->name.c_str());
    section(piccolo_text(strings, role >= 0 ? Str::map_step_interface : Str::map_step_buttons));
    const InputLayout* layout = picked_layout(window, context);
    if (role >= 0)
    {
        // The pad lights the button held now, as the positions mapped it.
        InputSource held = input_sdl_mapper_held_source(&window->mapper);
        pad_picture_draw(layout, input_map_control_for_source(held), held.type == InputSourceType::axis_minus, block * 1.5f);
        std::snprintf(text, sizeof(text), piccolo_text(strings, Str::map_press_for), piccolo_text(strings, ROLES[role].label));
    }
    else
    {
        pad_picture_draw(layout, control, false, block * 1.5f);   // the button to press, lit
        const char* label = control_label(layout, strings, control);
        if (input_map_control_is_stick(control))
        {
            std::snprintf(text, sizeof(text), "%s", label);
        }
        else
        {
            std::snprintf(text, sizeof(text), piccolo_text(strings, Str::map_press), label);
        }
    }
    ImGui::Spacing();
    section(text);
    if (role >= 0)
    {
        // Held 2 s to be taken: how far it has come (let go sooner to choose another).
        wrapped(piccolo_text(strings, Str::map_hold_hint));
        ImGui::ProgressBar(input_sdl_mapper_hold_progress(&window->mapper, context->now_ms),
                           ImVec2(-1.0f, ImGui::GetTextLineHeight()), "");
    }
    uint32_t left = input_sdl_mapper_seconds_left(&window->mapper, context->now_ms);
    if (left > 0)
    {
        std::snprintf(text, sizeof(text), piccolo_text(strings, Str::map_skip_in), static_cast<int>(left));
        ImGui::TextDisabled("%s", text);
    }
    footer();
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
    ImVec2 size(320.0f * block, 200.0f * block);
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
    std::snprintf(title, sizeof(title), "%s###map_controller",
                  piccolo_text(context->strings, window->offered ? Str::map_detected : Str::map_title));
    if (ImGui::Begin(title, nullptr, flags))
    {
        retro_ui_begin_window(scale);
        if (window->stage != window->drawn_stage)
        {
            if (window->stage == MapStage::appearance)
            {
                window->appearance_started_ms = context->now_ms;   // its countdown starts on entering it
            }
            window->drawn_stage = window->stage;
        }
        switch (window->stage)
        {
        case MapStage::offer:
            draw_offer(window, context);
            break;
        case MapStage::appearance:
            draw_appearance(window, context, block);
            break;
        case MapStage::intro_buttons:
            draw_intro_buttons(window, context, block);
            break;
        case MapStage::mapping:
            draw_mapping(window, context, block);
            break;
        case MapStage::intro_interface:
            draw_intro_interface(window, context, block);
            break;
        case MapStage::closed:
            break;
        }
        retro_ui_end_window();
    }
    ImGui::End();
    retro_ui_pop_theme();
}
