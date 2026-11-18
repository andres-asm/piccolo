#include "map_window.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <vector>

#include "imgui.h"
#include "input_sdl.h"
#include "pad_picture.h"
#include "pixel_font_imgui.h"
#include "piccolo_text.h"
#include "retro_ui.h"

constexpr uint64_t OFFER_MS = 15000;      // NOT NOW by itself after this long
constexpr uint64_t START_MS = 10000;      // a step's first page STARTs by itself after this long (untouched)
constexpr uint64_t QUESTION_MS = 15000;   // a STEP 4 question: NO by itself after this long

// The window's size and where the controller drawing sits, in UI blocks:
// always the same place, whatever the text above it.
constexpr float WINDOW_WIDTH = 320.0f;
constexpr float WINDOW_HEIGHT = 252.0f;
constexpr float PAD_TOP = 112.0f;
constexpr float PAD_UNIT = 1.25f;

// The names of the controls a layout doesn't name: the d-pad and the
// sticks (and any button a layout leaves without a label).
static const Str DEFAULT_LABELS[INPUT_MAP_CONTROL_COUNT] = {
    Str::map_bottom, Str::map_right, Str::map_left, Str::map_top, Str::map_dpad_up, Str::map_dpad_down,
    Str::map_dpad_left, Str::map_dpad_right, Str::map_title, Str::map_title, Str::map_title, Str::map_title,
    Str::map_title, Str::map_title, Str::map_title, Str::map_title, Str::map_guide, Str::map_left_x, Str::map_left_y,
    Str::map_right_x, Str::map_right_y, Str::map_paddle_1, Str::map_paddle_2, Str::map_paddle_3, Str::map_paddle_4,
    Str::map_l4, Str::map_r4, Str::map_other_button, Str::map_other_button, Str::map_other_button, Str::map_other_button,
};

// STEP 3: the actions each role binds, in the order asked.
struct MapRole
{
    Str         label;
    InputAction action;
    InputAction also;       // the same button goes here too (count = nothing more)
    bool        optional;   // a pad may not have it (shoulders for the pages)
};

static const MapRole ROLES[] = {
    {Str::map_role_up, InputAction::ui_up, InputAction::count, false},
    {Str::map_role_down, InputAction::ui_down, InputAction::count, false},
    {Str::map_role_left, InputAction::ui_left, InputAction::count, false},
    {Str::map_role_right, InputAction::ui_right, InputAction::count, false},
    {Str::map_role_page_left, InputAction::ui_page_left, InputAction::count, true},
    {Str::map_role_page_right, InputAction::ui_page_right, InputAction::count, true},
    {Str::map_role_accept, InputAction::ui_accept, InputAction::count, false},
    {Str::map_role_back, InputAction::ui_back, InputAction::count, false},
    // The main menu: opens it in the interface, and with HOTKEY ENABLE in a game.
    {Str::map_role_menu, InputAction::ui_open_menu, InputAction::hotkey_menu, false},
    {Str::map_role_modifier, InputAction::hotkey_enable, InputAction::count, false},
    {Str::map_role_quick_menu, InputAction::hotkey_quick_menu, InputAction::count, false},
};
constexpr uint32_t ROLE_COUNT = sizeof(ROLES) / sizeof(ROLES[0]);
static_assert(ROLE_COUNT <= MAP_WINDOW_ROLES_MAX, "MAP_WINDOW_ROLES_MAX too small");

static const Str MODE_NAMES[] = {Str::hotkey_mode_hold, Str::hotkey_mode_one, Str::hotkey_mode_stays};
static const InputHotkeyMode MODES[] = {InputHotkeyMode::hold, InputHotkeyMode::one_hotkey, InputHotkeyMode::stays_on};

static const ImVec4 YELLOW = ImVec4(1.0f, 220.0f / 255.0f, 80.0f / 255.0f, 1.0f);   // like the lit pad

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

// ---------------------------------------------------------------------------
// Opening, SDL's mapping, closing
// ---------------------------------------------------------------------------

// Everything a run of the wizard decides, back to nothing (opening, RESTART).
static void forget_choices(MapWindow* window)
{
    window->skip_type = false;
    window->skip_buttons = false;
    window->roles_done = false;
    for (uint32_t r = 0; r < MAP_WINDOW_ROLES_MAX; r++)
    {
        window->role_sources[r] = InputSource{};
    }
    window->pending_mapping.clear();
    window->interface_intro_done = false;
    window->want_guide = false;
    window->want_paddles = false;
    window->want_shoulders = false;
    window->want_others = false;
    window->extras_mapped = 0;
    window->layout = window->initial_layout;
    window->hotkey_mode = window->initial_hotkey_mode;
}

// SDL's mapping as it was when the wizard opened (nothing made is kept).
static void restore_mapping(MapWindow* window)
{
    if (window->mapping_changed)
    {
        SDL_SetGamepadMapping(static_cast<SDL_JoystickID>(window->joystick_id),
                              window->original_mapping.empty() ? nullptr : window->original_mapping.c_str());
        window->mapping_changed = false;
    }
    window->pending_mapping.clear();
}

// The mapping made, given to SDL now (not kept until FINISH).
static void use_mapping(MapWindow* window, const std::string& line)
{
    if (SDL_SetGamepadMapping(static_cast<SDL_JoystickID>(window->joystick_id), line.c_str()))
    {
        window->pending_mapping = line;
        window->mapping_changed = true;
    }
    else
    {
        SDL_Log("Mapping refused (%s): %s", SDL_GetError(), line.c_str());
    }
}

void map_window_offer(MapWindow* window, uint32_t joystick_id, bool needed, const InputLayouts* layouts,
                      const InputSystem* input, uint64_t now_ms)
{
    map_window_open(window, joystick_id, layouts, input);
    window->stage = MapStage::offer;
    window->offered = true;
    window->needed = needed;
    window->offer_started_ms = now_ms;
}

void map_window_open(MapWindow* window, uint32_t joystick_id, const InputLayouts* layouts, const InputSystem* input)
{
    window->stage = MapStage::type;
    window->offered = false;
    window->take_focus = true;
    window->joystick_id = joystick_id;
    window->name = input_sdl_joystick_name(joystick_id);
    window->guid = input_sdl_joystick_guid(joystick_id);
    window->needed = !SDL_IsGamepad(static_cast<SDL_JoystickID>(joystick_id));
    // Preselected from what SDL knows of it ("ps5", "xboxone"...), or from its profile.
    const char* type = SDL_GetGamepadStringForType(SDL_GetGamepadTypeForID(static_cast<SDL_JoystickID>(joystick_id)));
    const InputLayout* layout = layouts != nullptr ? input_layouts_for_type(layouts, type) : nullptr;
    window->initial_layout = layout != nullptr ? layout->id : std::string("Generic/generic");
    window->initial_hotkey_mode = InputHotkeyMode::hold;
    const InputProfile* profile = input != nullptr ? input_profile(input, window->guid) : nullptr;
    if (profile != nullptr)
    {
        if (layouts != nullptr && input_layouts_find(layouts, profile->appearance.c_str()) != nullptr)
        {
            window->initial_layout = profile->appearance;
        }
        window->initial_hotkey_mode = profile->hotkey_mode;
    }
    // SDL's mapping now, to put back unless FINISH.
    char* mapping = SDL_GetGamepadMappingForID(static_cast<SDL_JoystickID>(joystick_id));
    window->original_mapping = mapping != nullptr ? mapping : "";
    SDL_free(mapping);
    window->mapping_changed = false;
    forget_choices(window);
}

void map_window_close(MapWindow* window, InputSystem* input)
{
    input_sdl_mapper_cancel(&window->mapper);
    input_sdl_press_close(&window->press);
    restore_mapping(window);
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
// What FINISH keeps
// ---------------------------------------------------------------------------

// STEP 3's buttons as changes to the profile's bindings (and to the
// controller's own, if it is in use). Each role gets the button pressed for
// it and nothing else; a skipped one keeps its default.
static void apply_roles(MapWindow* window, const MapWindowContext* context)
{
    InputSystem* input = context->input;
    InputProfile* profile = input_profile_for(input, window->guid);
    for (uint32_t r = 0; r < ROLE_COUNT; r++)
    {
        InputSource source = window->role_sources[r];
        InputBindingOverride change = {};
        uint32_t count = 0;
        if (source.type != InputSourceType::none)
        {
            change.sources[count++] = source;
        }
        else if (!ROLES[r].optional)
        {
            continue;   // skipped: the default stays
        }
        InputAction targets[2] = {ROLES[r].action, ROLES[r].also};
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
}

// FINISH: the profile gets what the wizard made; SDL keeps the mapping.
static void finish(MapWindow* window, const MapWindowContext* context)
{
    InputSystem* input = context->input;
    InputProfile* profile = input_profile_for(input, window->guid);
    if (!window->pending_mapping.empty())
    {
        profile->sdl_mapping = window->pending_mapping;
        window->mapping_changed = false;   // stays in SDL
    }
    if (!window->skip_type)
    {
        profile->appearance = window->layout;
        profile->hotkey_mode = window->hotkey_mode;
        for (size_t c = 0; c < input->controllers.size(); c++)
        {
            if (input_controller_profile_id(input, static_cast<uint32_t>(c)) == window->guid)
            {
                input->controllers[c].hotkey_mode = window->hotkey_mode;
            }
        }
    }
    if (window->roles_done)
    {
        apply_roles(window, context);
    }
    window->profiles_dirty = true;
    char text[300] = {};
    std::snprintf(text, sizeof(text), piccolo_text(context->strings, Str::map_done), window->name.c_str());
    window->result = text;
    map_window_close(window, input);
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

// ---------------------------------------------------------------------------
// Page pieces
// ---------------------------------------------------------------------------

static void go(MapWindow* window, const MapWindowContext* context, MapStage stage)
{
    window->stage = stage;
    window->page_started_ms = context->now_ms;
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

// The controller's name, the step's title, what it is about.
static void header(const MapWindow* window, const Localization* strings, Str title, const char* text)
{
    retro_ui_separator_text(window->name.c_str());
    section(piccolo_text(strings, title));
    if (text != nullptr)
    {
        wrapped(text);
    }
}

// The controller drawing, always in the same place.
static void pad_area(const InputLayout* layout, PadView view, InputMapControl lit, bool minus, float block)
{
    float y = PAD_TOP * block;
    if (ImGui::GetCursorPosY() < y)
    {
        ImGui::SetCursorPosY(y);
    }
    pad_picture_draw_view(layout, view, lit, minus, PAD_UNIT * block);
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

// Seconds left of a countdown of `total_ms` started at `started_ms`.
static int seconds_left(uint64_t now_ms, uint64_t started_ms, uint64_t total_ms)
{
    uint64_t elapsed = now_ms - started_ms;
    return static_cast<int>((total_ms - (elapsed < total_ms ? elapsed : total_ms) + 999) / 1000);
}

// A button whose label counts down ("START (8)"): its id stays the same while
// the number changes, so a click on it isn't lost when the second ticks.
static bool countdown_button(const char* format, int seconds, const char* id)
{
    char label[128] = {};
    int written = std::snprintf(label, sizeof(label), format, seconds);
    if (written > 0 && static_cast<size_t>(written) < sizeof(label))
    {
        std::snprintf(label + written, sizeof(label) - static_cast<size_t>(written), "###%s", id);
    }
    return retro_ui_button(label);
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

// The prompt under the drawing while a step takes its inputs: what to hold,
// the ring closing, what holding means.
static void prompt(MapWindow* window, const MapWindowContext* context, const char* text, Str hint)
{
    section(text);
    char spinner[4] = {};
    pixel_font_imgui_spinner(input_sdl_mapper_hold_progress(&window->mapper, context->now_ms), spinner);
    if (spinner[0] != '\0')
    {
        ImGui::SameLine();
        ImGui::TextColored(YELLOW, "%s", spinner);
    }
    wrapped(piccolo_text(context->strings, hint));
}

// A step's own buttons while it takes its inputs: BACK (the previous input),
// RESET (the step's first input), SKIP (this one; counting down when it
// skips by itself), CANCEL (the whole wizard, nothing kept).
static void step_buttons(MapWindow* window, const MapWindowContext* context)
{
    const Localization* strings = context->strings;
    footer();
    ImGui::BeginDisabled(input_sdl_mapper_at_phase_start(&window->mapper));
    if (retro_ui_button(piccolo_text(strings, Str::map_back)))
    {
        input_sdl_mapper_back(&window->mapper, context->now_ms);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::map_reset)))
    {
        input_sdl_mapper_restart_phase(&window->mapper, context->now_ms);
    }
    ImGui::SameLine();
    uint32_t left = input_sdl_mapper_seconds_left(&window->mapper, context->now_ms);
    bool skip = left > 0 ? countdown_button(piccolo_text(strings, Str::map_skip_n), static_cast<int>(left), "skip")
                         : retro_ui_button(piccolo_text(strings, Str::map_skip));
    if (skip)
    {
        input_sdl_mapper_skip(&window->mapper, context->now_ms);
    }
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::map_cancel)))
    {
        map_window_close(window, context->input);   // nothing kept: SDL's mapping as it was
    }
}

// ---------------------------------------------------------------------------
// The mapper
// ---------------------------------------------------------------------------

// Starts the mapper on STEP 2's positions (the layout's), or on none (STEP 2
// skipped, or STEP 3 again: SDL's mapping now is loaded, so STEP 3 finds its
// buttons there), then STEP 3's roles. The router is suspended from then on.
static void start_mapper(MapWindow* window, const MapWindowContext* context, bool positions)
{
    const InputLayout* layout = picked_layout(window, context);
    bool wanted[INPUT_MAP_CONTROL_COUNT] = {};
    for (uint32_t c = 0; positions && c < static_cast<uint32_t>(INPUT_MAP_FIRST_EXTRA); c++)
    {
        wanted[c] = layout == nullptr || input_layout_has(layout, static_cast<InputMapControl>(c));
    }
    wanted[static_cast<uint32_t>(InputMapControl::guide)] = false;   // GUIDE / HOME: STEP 4's
    bool optional[ROLE_COUNT] = {};
    for (uint32_t r = 0; r < ROLE_COUNT; r++)
    {
        optional[r] = ROLES[r].optional;
    }
    if (!input_sdl_mapper_begin(&window->mapper, window->joystick_id, ROLE_COUNT, optional, wanted, context->now_ms))
    {
        map_window_close(window, context->input);   // went away
        return;
    }
    window->mapper.hold_ms = context->input->settings.hold_ms;   // SETTINGS > INPUT
    window->mapper.optional_wait_ms = context->input->settings.press_ms;
    if (!positions)
    {
        char* mapping = SDL_GetGamepadMappingForID(static_cast<SDL_JoystickID>(window->joystick_id));
        input_sdl_mapper_load(&window->mapper, mapping);
        SDL_free(mapping);
    }
    window->interface_intro_done = false;
    go(window, context, MapStage::mapping);
    context->input->suspended = true;   // its presses are the mapping's alone
}

// After STEP 3: STEP 4 from SETTINGS, else the summary.
static void after_interface(MapWindow* window, const MapWindowContext* context)
{
    input_sdl_mapper_cancel(&window->mapper);
    context->input->suspended = false;
    go(window, context, window->offered ? MapStage::summary : MapStage::extras_guide);
}

// STEP 3 done: its buttons kept for FINISH.
static void roles_done(MapWindow* window, const MapWindowContext* context)
{
    for (uint32_t r = 0; r < ROLE_COUNT; r++)
    {
        window->role_sources[r] = input_sdl_mapper_role_source(&window->mapper, r);
    }
    window->roles_done = true;
    after_interface(window, context);
}

// A wizard BACK into STEP 2: what it made goes (SDL's mapping as it was).
static void back_to_buttons(MapWindow* window, const MapWindowContext* context)
{
    input_sdl_mapper_cancel(&window->mapper);
    context->input->suspended = false;
    restore_mapping(window);
    window->skip_buttons = false;
    go(window, context, MapStage::intro_buttons);
}

// STEP 4's inputs: the extras answered YES, added to the mapping it has now.
static void start_extras(MapWindow* window, const MapWindowContext* context)
{
    bool wanted[INPUT_MAP_CONTROL_COUNT] = {};
    bool any = false;
    wanted[static_cast<uint32_t>(InputMapControl::guide)] = window->want_guide;
    any = window->want_guide;
    for (uint32_t c = static_cast<uint32_t>(INPUT_MAP_FIRST_EXTRA); c < INPUT_MAP_CONTROL_COUNT; c++)
    {
        InputMapControl control = static_cast<InputMapControl>(c);
        bool paddle = control == InputMapControl::paddle1 || control == InputMapControl::paddle2 ||
                      control == InputMapControl::paddle3 || control == InputMapControl::paddle4;
        bool shoulder = control == InputMapControl::l4 || control == InputMapControl::r4;
        wanted[c] = paddle ? window->want_paddles : (shoulder ? window->want_shoulders : window->want_others);
        any = any || wanted[c];
    }
    if (!any)
    {
        go(window, context, MapStage::summary);
        return;
    }
    if (!input_sdl_mapper_begin(&window->mapper, window->joystick_id, 0, nullptr, wanted, context->now_ms))
    {
        map_window_close(window, context->input);   // went away
        return;
    }
    window->mapper.hold_ms = context->input->settings.hold_ms;
    window->mapper.optional_wait_ms = context->input->settings.press_ms;
    window->mapper.end_on_skip_from = InputMapControl::misc1;   // other buttons: until none comes
    char* mapping = SDL_GetGamepadMappingForID(static_cast<SDL_JoystickID>(window->joystick_id));
    input_sdl_mapper_load(&window->mapper, mapping);
    SDL_free(mapping);
    go(window, context, MapStage::extras_mapping);
    context->input->suspended = true;
}

// STEP 4's inputs done: the extras into the mapping SDL uses now.
static void extras_done(MapWindow* window, const MapWindowContext* context)
{
    window->extras_mapped = 0;
    for (uint32_t c = 0; c < INPUT_MAP_CONTROL_COUNT; c++)
    {
        if (window->mapper.wanted[c] && !window->mapper.elements[c].empty())
        {
            window->extras_mapped++;
        }
    }
    std::string line;
    if (window->extras_mapped > 0 && input_sdl_mapper_line(&window->mapper, &line))
    {
        use_mapping(window, line);
    }
    input_sdl_mapper_cancel(&window->mapper);
    context->input->suspended = false;
    go(window, context, MapStage::summary);
}

// ---------------------------------------------------------------------------
// The pages
// ---------------------------------------------------------------------------

static void draw_offer(MapWindow* window, const MapWindowContext* context)
{
    const Localization* strings = context->strings;
    retro_ui_separator_text(window->name.c_str());
    wrapped(piccolo_text(strings, Str::map_offer_text));
    if (window->needed)
    {
        ImGui::Spacing();
        wrapped(piccolo_text(strings, Str::map_offer_needed));
    }
    ImGui::Spacing();
    wrapped(piccolo_text(strings, Str::map_offer_any_button));
    footer();
    // Any button on the new controller is MAP, whether SDL knows it or not.
    bool pressed = raw_press(window);
    if (retro_ui_button(piccolo_text(strings, Str::map_map)) || pressed)
    {
        go(window, context, MapStage::type);
        return;
    }
    ImGui::SameLine();
    if (countdown_button(piccolo_text(strings, Str::map_not_now), seconds_left(context->now_ms, window->offer_started_ms, OFFER_MS),
                         "not_now") ||
        context->now_ms - window->offer_started_ms >= OFFER_MS)
    {
        not_now(window, context);
    }
}

// STEP 1: the type (LABELS, LAYOUT) and HOTKEY MODE.
static void draw_type(MapWindow* window, const MapWindowContext* context, float block)
{
    const Localization* strings = context->strings;
    header(window, strings, Str::map_step_type, nullptr);
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
    // HOTKEY MODE: how its HOTKEY ENABLE (asked in STEP 3) turns hotkeys on.
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
    wrapped(piccolo_text(strings, window->needed ? Str::map_type_goes_on_any : Str::map_type_goes_on));
    pad_area(layout, PadView::front, InputMapControl::count, false, block);
    footer();
    // Choosing in a list (or opening one) starts the countdown again; the
    // buttons below don't (their press is a choice, not a touch).
    if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
    {
        window->page_started_ms = context->now_ms;
    }
    bool pressed = window->needed && raw_press(window);
    bool timed_out = context->now_ms - window->page_started_ms >= START_MS;
    if (countdown_button(piccolo_text(strings, Str::map_start), seconds_left(context->now_ms, window->page_started_ms, START_MS),
                         "start") ||
        pressed || timed_out)
    {
        window->skip_type = false;
        go(window, context, MapStage::intro_buttons);
        return;
    }
    // SKIP: keep its type (only for a controller already mapped).
    ImGui::SameLine();
    ImGui::BeginDisabled(!SDL_IsGamepad(static_cast<SDL_JoystickID>(window->joystick_id)));
    bool skip = retro_ui_button(piccolo_text(strings, Str::map_skip));
    ImGui::EndDisabled();
    if (skip)
    {
        window->skip_type = true;
        window->layout = window->initial_layout;
        window->hotkey_mode = window->initial_hotkey_mode;
        go(window, context, MapStage::intro_buttons);
        return;
    }
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::map_cancel)))
    {
        map_window_close(window, context->input);
    }
}

// STEP 2's first page: START begins its inputs.
static void draw_intro_buttons(MapWindow* window, const MapWindowContext* context, float block)
{
    const Localization* strings = context->strings;
    header(window, strings, Str::map_step_buttons, piccolo_text(strings, Str::map_step_buttons_text));
    wrapped(piccolo_text(strings, Str::map_starts_any));
    pad_area(picked_layout(window, context), PadView::front, InputMapControl::count, false, block);
    footer();
    if (retro_ui_button(piccolo_text(strings, Str::map_back)))
    {
        go(window, context, MapStage::type);
        return;
    }
    ImGui::SameLine();
    if (countdown_button(piccolo_text(strings, Str::map_start), seconds_left(context->now_ms, window->page_started_ms, START_MS),
                         "start") ||
        raw_press(window) || context->now_ms - window->page_started_ms >= START_MS)
    {
        window->skip_buttons = false;
        start_mapper(window, context, true);
        return;
    }
    // SKIP: keep SDL's mapping (only one SDL knows: STEP 3 needs its buttons mapped).
    ImGui::SameLine();
    ImGui::BeginDisabled(!SDL_IsGamepad(static_cast<SDL_JoystickID>(window->joystick_id)));
    bool skip = retro_ui_button(piccolo_text(strings, Str::map_skip));
    ImGui::EndDisabled();
    if (skip)
    {
        window->skip_buttons = true;
        start_mapper(window, context, false);
        return;
    }
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::map_cancel)))
    {
        map_window_close(window, context->input);
    }
}

// STEP 3's first page (the mapper waits on the first role).
static void draw_intro_interface(MapWindow* window, const MapWindowContext* context, float block)
{
    const Localization* strings = context->strings;
    header(window, strings, Str::map_step_interface, piccolo_text(strings, Str::map_step_interface_text));
    wrapped(piccolo_text(strings, Str::map_starts_any));   // the router is suspended: no controller drives the page
    pad_area(picked_layout(window, context), PadView::front, InputMapControl::count, false, block);
    footer();
    if (retro_ui_button(piccolo_text(strings, Str::map_back)))
    {
        back_to_buttons(window, context);
        return;
    }
    ImGui::SameLine();
    if (countdown_button(piccolo_text(strings, Str::map_start), seconds_left(context->now_ms, window->page_started_ms, START_MS),
                         "start") ||
        raw_press(window) || context->now_ms - window->page_started_ms >= START_MS)
    {
        window->interface_intro_done = true;
        input_sdl_mapper_restart_step(&window->mapper, context->now_ms);
        window->stage = MapStage::mapping;
        return;
    }
    // SKIP: keep its interface buttons.
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::map_skip)))
    {
        window->roles_done = false;
        after_interface(window, context);
        return;
    }
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::map_cancel)))
    {
        map_window_close(window, context->input);
    }
}

// STEP 2's inputs, then STEP 3's.
static void draw_mapping(MapWindow* window, const MapWindowContext* context, float block)
{
    const Localization* strings = context->strings;
    if (input_sdl_mapper_update(&window->mapper, context->now_ms))
    {
        roles_done(window, context);
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
        // STEP 2 is done: its mapping, given to SDL now (kept on FINISH).
        std::string line;
        if (!window->skip_buttons && input_sdl_mapper_line(&window->mapper, &line))
        {
            use_mapping(window, line);
        }
        go(window, context, MapStage::intro_interface);
        return;
    }
    const InputLayout* layout = picked_layout(window, context);
    char text[300] = {};
    header(window, strings, role >= 0 ? Str::map_step_interface : Str::map_step_buttons, nullptr);
    Str hint = Str::map_hold_hint;
    if (role >= 0)
    {
        // The drawing lights the button held now, as STEP 2 mapped it.
        InputSource held = input_sdl_mapper_held_source(&window->mapper);
        pad_area(layout, PadView::front, input_map_control_for_source(held), held.type == InputSourceType::axis_minus, block);
        std::snprintf(text, sizeof(text), piccolo_text(strings, Str::map_press_for), piccolo_text(strings, ROLES[role].label));
    }
    else
    {
        InputMapControl control = input_sdl_mapper_control(&window->mapper);
        pad_area(layout, PadView::front, control, false, block);   // the button to press, lit
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
    prompt(window, context, text, hint);
    step_buttons(window, context);
}

// A STEP 4 question: YES, or NO (by itself after a while).
static void draw_question(MapWindow* window, const MapWindowContext* context, float block, Str question, PadView view,
                          InputMapControl lit, bool* answer, MapStage back, MapStage next)
{
    const Localization* strings = context->strings;
    header(window, strings, Str::map_step_extras, piccolo_text(strings, question));
    pad_area(picked_layout(window, context), view, lit, false, block);
    footer();
    if (retro_ui_button(piccolo_text(strings, Str::map_back)))
    {
        if (back == MapStage::intro_interface)
        {
            window->roles_done = false;
            start_mapper(window, context, false);   // STEP 3 again, on the mapping it has now
        }
        else
        {
            go(window, context, back);
        }
        return;
    }
    ImGui::SameLine();
    bool answered = false;
    if (retro_ui_button(piccolo_text(strings, Str::map_yes)))
    {
        *answer = true;
        answered = true;
    }
    ImGui::SameLine();
    if (!answered && (countdown_button(piccolo_text(strings, Str::map_no),
                                       seconds_left(context->now_ms, window->page_started_ms, QUESTION_MS), "no") ||
                      context->now_ms - window->page_started_ms >= QUESTION_MS))
    {
        *answer = false;
        answered = true;
    }
    if (answered)
    {
        if (next == MapStage::extras_mapping)
        {
            start_extras(window, context);
        }
        else
        {
            go(window, context, next);
        }
        return;
    }
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::map_cancel)))
    {
        map_window_close(window, context->input);
    }
}

// STEP 4's inputs: each extra answered YES.
static void draw_extras_mapping(MapWindow* window, const MapWindowContext* context, float block)
{
    const Localization* strings = context->strings;
    if (input_sdl_mapper_update(&window->mapper, context->now_ms))
    {
        extras_done(window, context);
        return;
    }
    if (!window->mapper.active)
    {
        map_window_close(window, context->input);   // went away
        return;
    }
    InputMapControl control = input_sdl_mapper_control(&window->mapper);
    const InputLayout* layout = picked_layout(window, context);
    char text[300] = {};
    header(window, strings, Str::map_step_extras, nullptr);
    bool paddle = control == InputMapControl::paddle1 || control == InputMapControl::paddle2 ||
                  control == InputMapControl::paddle3 || control == InputMapControl::paddle4;
    bool shoulder = control == InputMapControl::l4 || control == InputMapControl::r4;
    pad_area(layout, paddle ? PadView::back : (shoulder ? PadView::extra_shoulders : PadView::front), control, false, block);
    bool other = static_cast<uint32_t>(control) >= static_cast<uint32_t>(InputMapControl::misc1) && control != InputMapControl::count;
    std::snprintf(text, sizeof(text), piccolo_text(strings, Str::map_press), control_label(layout, strings, control));
    prompt(window, context, text, other ? Str::map_extras_others_text : Str::map_hold_hint);
    step_buttons(window, context);
}

// The last page: what FINISH keeps.
static void draw_summary(MapWindow* window, const MapWindowContext* context, float block)
{
    const Localization* strings = context->strings;
    header(window, strings, Str::map_summary, nullptr);
    float label_width = 120.0f * block;
    const char* changed = piccolo_text(strings, Str::map_summary_changed);
    const char* unchanged = piccolo_text(strings, Str::map_summary_unchanged);
    const InputLayout* layout = picked_layout(window, context);

    ImGui::TextUnformatted(piccolo_text(strings, Str::map_summary_type));
    ImGui::SameLine(label_width);
    ImGui::TextUnformatted(window->skip_type || layout == nullptr ? unchanged : layout->name.c_str());
    ImGui::TextUnformatted(piccolo_text(strings, Str::map_summary_buttons));
    ImGui::SameLine(label_width);
    ImGui::TextUnformatted(window->skip_buttons ? unchanged : changed);
    ImGui::TextUnformatted(piccolo_text(strings, Str::map_summary_interface));
    ImGui::SameLine(label_width);
    ImGui::TextUnformatted(window->roles_done ? changed : unchanged);
    if (!window->offered)
    {
        char text[64] = {};
        std::snprintf(text, sizeof(text), piccolo_text(strings, Str::map_summary_added), static_cast<int>(window->extras_mapped));
        ImGui::TextUnformatted(piccolo_text(strings, Str::map_summary_extras));
        ImGui::SameLine(label_width);
        ImGui::TextUnformatted(window->extras_mapped > 0 ? text : unchanged);
    }
    ImGui::Spacing();
    wrapped(piccolo_text(strings, Str::map_summary_text));
    footer();
    if (retro_ui_button(piccolo_text(strings, Str::map_restart)))
    {
        restore_mapping(window);
        forget_choices(window);
        go(window, context, MapStage::type);
        return;
    }
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::map_cancel)))
    {
        map_window_close(window, context->input);
        return;
    }
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::map_finish)))
    {
        finish(window, context);
    }
}

// Which page of the wizard this is, as a row of small squares above the
// buttons (the current one lit): the steps, then the summary. None on OFFER.
static void draw_page_indicator(const MapWindow* window)
{
    int32_t page = -1;
    switch (window->stage)
    {
        case MapStage::type:            page = 0; break;
        case MapStage::intro_buttons:   page = 1; break;
        case MapStage::mapping:         page = input_sdl_mapper_role(&window->mapper) >= 0 ? 2 : 1; break;
        case MapStage::intro_interface: page = 2; break;
        case MapStage::extras_guide:
        case MapStage::extras_paddles:
        case MapStage::extras_shoulders:
        case MapStage::extras_others:
        case MapStage::extras_mapping:  page = 3; break;
        case MapStage::summary:         page = window->offered ? 3 : 4; break;
        default:                        break;
    }
    if (page < 0)
    {
        return;
    }
    const int32_t PAGES = window->offered ? 4 : 5;   // STEP 4 only from SETTINGS
    const ImGuiStyle& style = ImGui::GetStyle();
    float size = static_cast<float>(static_cast<int>(ImGui::GetFontSize() * 0.75f));
    float gap = size;
    float width = static_cast<float>(PAGES) * size + static_cast<float>(PAGES - 1) * gap;
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    float x = window_pos.x + static_cast<float>(static_cast<int>((window_size.x - width) * 0.5f));
    // A gap between it and the buttons.
    float y = window_pos.y + window_size.y - style.WindowPadding.y - ImGui::GetFrameHeight() - style.ItemSpacing.y - size * 2.0f;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    for (int32_t p = 0; p < PAGES; p++)
    {
        float px = x + static_cast<float>(p) * (size + gap);
        ImU32 color = p == page ? IM_COL32(255, 220, 80, 255) : ImGui::GetColorU32(ImGuiCol_TextDisabled);
        draw->AddRectFilled(ImVec2(px, y), ImVec2(px + size, y + size), color);
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
    ImVec2 size(WINDOW_WIDTH * block, WINDOW_HEIGHT * block);
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
            if (window->drawn_stage == MapStage::closed || window->drawn_stage == MapStage::offer)
            {
                window->page_started_ms = context->now_ms;   // the first page's countdown starts on showing it
            }
            window->drawn_stage = window->stage;
        }
        draw_page_indicator(window);
        switch (window->stage)
        {
        case MapStage::offer:
            draw_offer(window, context);
            break;
        case MapStage::type:
            draw_type(window, context, block);
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
        case MapStage::extras_guide:
            draw_question(window, context, block, Str::map_extras_guide, PadView::front, InputMapControl::guide,
                          &window->want_guide, MapStage::intro_interface, MapStage::extras_paddles);
            break;
        case MapStage::extras_paddles:
            draw_question(window, context, block, Str::map_extras_paddles, PadView::back, InputMapControl::count,
                          &window->want_paddles, MapStage::extras_guide, MapStage::extras_shoulders);
            break;
        case MapStage::extras_shoulders:
            draw_question(window, context, block, Str::map_extras_shoulders, PadView::extra_shoulders, InputMapControl::count,
                          &window->want_shoulders, MapStage::extras_paddles, MapStage::extras_others);
            break;
        case MapStage::extras_others:
            draw_question(window, context, block, Str::map_extras_others, PadView::front, InputMapControl::count,
                          &window->want_others, MapStage::extras_shoulders, MapStage::extras_mapping);
            break;
        case MapStage::extras_mapping:
            draw_extras_mapping(window, context, block);
            break;
        case MapStage::summary:
            draw_summary(window, context, block);
            break;
        case MapStage::closed:
            break;
        }
        retro_ui_end_window();
    }
    ImGui::End();
    retro_ui_pop_theme();
}
