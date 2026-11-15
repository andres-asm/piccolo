#include "input_window.h"

#include <SDL3/SDL.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

#include "imgui.h"
#include "piccolo_text.h"
#include "piccolo_settings.h"
#include "retro_ui.h"
#include "settings_ui.h"

// SET gives up after this long without a key or button.
constexpr uint64_t CAPTURE_TIMEOUT_MS = 5000;
constexpr uint16_t HID_ESCAPE = 41;

// The label of each InputAction, in its order.
static const Str ACTION_LABELS[INPUT_ACTION_COUNT] = {
    Str::input_action_pad_b, Str::input_action_pad_y, Str::input_action_pad_select, Str::input_action_pad_start,
    Str::input_action_pad_up, Str::input_action_pad_down, Str::input_action_pad_left, Str::input_action_pad_right,
    Str::input_action_pad_a, Str::input_action_pad_x, Str::input_action_pad_l, Str::input_action_pad_r,
    Str::input_action_pad_l2, Str::input_action_pad_r2, Str::input_action_pad_l3, Str::input_action_pad_r3,
    Str::input_action_left_x_plus, Str::input_action_left_x_minus, Str::input_action_left_y_plus,
    Str::input_action_left_y_minus, Str::input_action_right_x_plus, Str::input_action_right_x_minus,
    Str::input_action_right_y_plus, Str::input_action_right_y_minus,
    Str::input_action_ui_up, Str::input_action_ui_down, Str::input_action_ui_left, Str::input_action_ui_right,
    Str::input_action_ui_accept, Str::input_action_ui_back, Str::input_action_ui_page_left,
    Str::input_action_ui_page_right, Str::input_action_ui_open_menu,
    Str::input_action_hotkey_enable, Str::input_action_hotkey_menu, Str::input_action_hotkey_quick_menu,
    Str::input_action_hotkey_save_state, Str::input_action_hotkey_load_state, Str::input_action_hotkey_fast_forward,
};

static const char* with_id(char* buffer, size_t size, const char* text, const char* id)
{
    std::snprintf(buffer, size, "%s###%s", text, id);
    return buffer;
}

static void upper(char* text)
{
    for (char* c = text; *c != '\0'; c++)
    {
        *c = static_cast<char>(std::toupper(static_cast<unsigned char>(*c)));
    }
}

static const char* face_button_label(SDL_Gamepad* gamepad, InputButton button, const char* fallback)
{
    if (gamepad == nullptr)
    {
        return fallback;
    }
    switch (SDL_GetGamepadButtonLabel(gamepad, static_cast<SDL_GamepadButton>(button)))
    {
    case SDL_GAMEPAD_BUTTON_LABEL_A:        return "A";
    case SDL_GAMEPAD_BUTTON_LABEL_B:        return "B";
    case SDL_GAMEPAD_BUTTON_LABEL_X:        return "X";
    case SDL_GAMEPAD_BUTTON_LABEL_Y:        return "Y";
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS:    return "CROSS";
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:   return "CIRCLE";
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:   return "SQUARE";
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE: return "TRIANGLE";
    default:                                return fallback;
    }
}

void input_window_source_label(const InputSdl* sdl, const InputDevice* device, InputSource source, char* out, size_t out_size)
{
    SDL_Gamepad* gamepad = device != nullptr ? input_sdl_gamepad(sdl, device->id) : nullptr;
    const char* text = "?";
    char key[32] = {};
    switch (source.type)
    {
    case InputSourceType::key:
    {
        const char* name = input_key_name(source.code);
        if (name != nullptr)
        {
            std::snprintf(key, sizeof(key), "%s", name);
        }
        else
        {
            std::snprintf(key, sizeof(key), "KEY %u", static_cast<unsigned>(source.code));
        }
        upper(key);
        text = key;
        break;
    }
    case InputSourceType::button:
        switch (static_cast<InputButton>(source.code))
        {
        case InputButton::south:          text = face_button_label(gamepad, InputButton::south, "BOTTOM BUTTON"); break;
        case InputButton::east:           text = face_button_label(gamepad, InputButton::east, "RIGHT BUTTON"); break;
        case InputButton::west:           text = face_button_label(gamepad, InputButton::west, "LEFT BUTTON"); break;
        case InputButton::north:          text = face_button_label(gamepad, InputButton::north, "TOP BUTTON"); break;
        case InputButton::back:           text = "SELECT"; break;
        case InputButton::guide:          text = "HOME"; break;
        case InputButton::start:          text = "START"; break;
        case InputButton::left_stick:     text = "L3"; break;
        case InputButton::right_stick:    text = "R3"; break;
        case InputButton::left_shoulder:  text = "L"; break;
        case InputButton::right_shoulder: text = "R"; break;
        case InputButton::dpad_up:        text = "D-PAD UP"; break;
        case InputButton::dpad_down:      text = "D-PAD DOWN"; break;
        case InputButton::dpad_left:      text = "D-PAD LEFT"; break;
        case InputButton::dpad_right:     text = "D-PAD RIGHT"; break;
        case InputButton::misc1:          text = "MISC"; break;
        case InputButton::right_paddle1:  text = "RIGHT PADDLE 1"; break;
        case InputButton::left_paddle1:   text = "LEFT PADDLE 1"; break;
        case InputButton::right_paddle2:  text = "RIGHT PADDLE 2"; break;
        case InputButton::left_paddle2:   text = "LEFT PADDLE 2"; break;
        case InputButton::touchpad:       text = "TOUCHPAD"; break;
        case InputButton::count:          break;
        }
        break;
    case InputSourceType::axis_plus:
    case InputSourceType::axis_minus:
    {
        bool plus = source.type == InputSourceType::axis_plus;
        switch (static_cast<InputAxis>(source.code))
        {
        case InputAxis::left_x:        text = plus ? "LEFT STICK RIGHT" : "LEFT STICK LEFT"; break;
        case InputAxis::left_y:        text = plus ? "LEFT STICK DOWN" : "LEFT STICK UP"; break;
        case InputAxis::right_x:       text = plus ? "RIGHT STICK RIGHT" : "RIGHT STICK LEFT"; break;
        case InputAxis::right_y:       text = plus ? "RIGHT STICK DOWN" : "RIGHT STICK UP"; break;
        case InputAxis::left_trigger:  text = "L2"; break;
        case InputAxis::right_trigger: text = "R2"; break;
        case InputAxis::count:         break;
        }
        break;
    }
    case InputSourceType::none:
        text = "";
        break;
    }
    std::snprintf(out, out_size, "%s", text);
}

void input_window_open(InputWindow* window)
{
    window->open = true;
    window->choose_scope = true;
    window->mapping = UINT32_MAX;   // port 1's controller comes up
}

void input_window_controller_defaults(const InputSystem* input, uint32_t controller, InputBindingSet* set)
{
    *set = InputBindingSet{};
    if (controller >= input->controllers.size())
    {
        return;
    }
    const InputDevice* device = input_devices_find_const(&input->devices, input->controllers[controller].device_id);
    if (device == nullptr)
    {
        return;
    }
    bool first = input_first_controller(input, device->id) == static_cast<int32_t>(controller);
    if (device->kind == InputDeviceKind::gamepad || first)
    {
        input_default_bindings(device->kind, set);   // more keyboard players start with no keys
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static float button_width(const char* label)
{
    return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
}

// Moves the cursor so `width` fits at the right end of the line.
static void align_right(float width)
{
    float x = ImGui::GetWindowContentRegionMax().x - width;
    if (x > ImGui::GetCursorPosX())
    {
        ImGui::SameLine(x);
    }
    else
    {
        ImGui::SameLine();
    }
}

static bool is_keyboard(const InputSystem* input, uint32_t controller)
{
    const InputDevice* device = input_devices_find_const(&input->devices, input->controllers[controller].device_id);
    return device != nullptr && device->kind == InputDeviceKind::keyboard;
}

static void controller_label(const InputWindowContext* context, uint32_t controller, char* out, size_t out_size)
{
    const InputSystem* input = context->input;
    const InputDevice* device = input_devices_find_const(&input->devices, input->controllers[controller].device_id);
    const char* state = "";
    if (device == nullptr || !device->connected)
    {
        state = piccolo_text(context->strings, Str::input_device_disconnected);
    }
    else if (input_devices_ignored(device))
    {
        state = piccolo_text(context->strings, Str::input_device_ignored);
    }
    std::snprintf(out, out_size, state[0] != '\0' ? "%s (%s)" : "%s%s", input->controllers[controller].name.c_str(), state);
}

// The same sources, in any order?
static bool same_sources(const InputSource* a, const InputSource* b)
{
    uint32_t count_a = 0;
    uint32_t count_b = 0;
    for (uint32_t s = 0; s < INPUT_SOURCES_PER_ACTION; s++)
    {
        count_a += a[s].type != InputSourceType::none ? 1u : 0u;
        count_b += b[s].type != InputSourceType::none ? 1u : 0u;
        bool found = a[s].type == InputSourceType::none;
        for (uint32_t t = 0; t < INPUT_SOURCES_PER_ACTION && !found; t++)
        {
            found = b[t].type != InputSourceType::none && input_source_equal(a[s], b[t]);
        }
        if (!found)
        {
            return false;
        }
    }
    return count_a == count_b;
}

// The NAME field of a controller (written when the field is left).
static void name_field(InputWindow* window, const InputWindowContext* context, uint32_t controller, float width)
{
    InputSystem* input = context->input;
    if (window->names.size() != input->controllers.size())
    {
        window->names.resize(input->controllers.size());
    }
    char buffer[64] = {};
    std::snprintf(buffer, sizeof(buffer), "%s", window->names[controller].c_str());
    ImGui::SetNextItemWidth(width);
    retro_ui_push_field_text();
    if (ImGui::InputText("##name", buffer, sizeof(buffer)))
    {
        window->names[controller] = buffer;
    }
    retro_ui_pop_field_text();
    if (ImGui::IsItemDeactivatedAfterEdit() && !window->names[controller].empty())
    {
        input->controllers[controller].name = window->names[controller];
        window->dirty = true;
    }
    if (!ImGui::IsItemActive())
    {
        window->names[controller] = input->controllers[controller].name;
    }
}

// ---------------------------------------------------------------------------
// SETTINGS
// ---------------------------------------------------------------------------

// The SCOPE and one or more settings rows (the settings of SETTINGS and of
// HOTKEYS share the window's scope).
static void draw_scoped_settings(InputWindow* window, const InputWindowContext* context, const char* const* keys,
                                 uint32_t count)
{
    settings_ui_scope(context->settings, context->strings, context->core_name, context->game_name, &window->scope,
                      &window->choose_scope);
    SettingsUiContext ui = {};
    ui.settings = context->settings;
    ui.strings = context->strings;
    ui.scope = window->scope;
    ui.label_width = settings_ui_label_width(context->settings, context->strings);
    for (uint32_t k = 0; k < count; k++)
    {
        settings_ui_row(&ui, keys[k], nullptr);
    }
}

static void draw_settings(InputWindow* window, const InputWindowContext* context)
{
    static const char* const KEYS[] = {PICCOLO_INPUT_SHARE_PORT1};
    draw_scoped_settings(window, context, KEYS, 1);
}

// ---------------------------------------------------------------------------
// RETROPAD / INTERFACE / HOTKEYS
// ---------------------------------------------------------------------------

static void start_capture(InputWindow* window, const InputWindowContext* context, uint32_t controller, InputAction action)
{
    window->capturing = true;
    window->capture_action = action;
    window->capture_controller = controller;
    window->capture_started_ms = context->now_ms;
    input_capture_begin(context->input, context->input->controllers[controller].device_id);
}

static void binding_row(InputWindow* window, const InputWindowContext* context, uint32_t controller_index,
                        const InputBindingSet* defaults, InputAction action, float label_width)
{
    InputSystem* input = context->input;
    InputController* controller = &input->controllers[controller_index];
    const InputDevice* device = input_devices_find_const(&input->devices, controller->device_id);
    const Localization* strings = context->strings;
    uint32_t index = static_cast<uint32_t>(action);
    bool changed = !same_sources(controller->bindings.sources[index], defaults->sources[index]);

    ImGui::PushID(static_cast<int>(index));
    ImGui::AlignTextToFramePadding();
    if (changed)
    {
        // Changed from the defaults: highlighted, like an override.
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive), "%s", piccolo_text(strings, ACTION_LABELS[index]));
    }
    else
    {
        ImGui::TextUnformatted(piccolo_text(strings, ACTION_LABELS[index]));
    }
    ImGui::SameLine(label_width);
    if (window->capturing && window->capture_action == action && window->capture_controller == controller_index)
    {
        retro_ui_push_field_text();
        ImGui::TextUnformatted(piccolo_text(strings, Str::input_press));
        retro_ui_pop_field_text();
    }
    else
    {
        std::string text;
        for (uint32_t s = 0; s < INPUT_SOURCES_PER_ACTION; s++)
        {
            InputSource source = controller->bindings.sources[index][s];
            if (source.type == InputSourceType::none)
            {
                continue;
            }
            char name[48] = {};
            input_window_source_label(context->sdl, device, source, name, sizeof(name));
            text += text.empty() ? "" : ", ";
            text += name;
        }
        if (text.empty())
        {
            ImGui::TextDisabled("%s", piccolo_text(strings, Str::input_none));
        }
        else
        {
            ImGui::TextUnformatted(text.c_str());
        }
    }
    const char* set = piccolo_text(strings, Str::input_set);
    const char* clear = piccolo_text(strings, Str::input_clear);
    const char* reset = piccolo_text(strings, Str::input_reset);
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    align_right(button_width(set) + button_width(clear) + button_width(reset) + spacing * 2.0f);
    ImGui::BeginDisabled(window->capturing || device == nullptr || !device->connected);
    if (retro_ui_button(set))
    {
        window->bind_all = false;
        start_capture(window, context, controller_index, action);
    }
    ImGui::SameLine();
    if (retro_ui_button(clear))
    {
        input_unbind_all(&controller->bindings, action);
        window->dirty = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (changed)
    {
        ImGui::BeginDisabled(window->capturing);
        if (retro_ui_button(reset))
        {
            input_unbind_all(&controller->bindings, action);
            for (uint32_t s = 0; s < INPUT_SOURCES_PER_ACTION; s++)
            {
                input_bind(&controller->bindings, action, defaults->sources[index][s]);
            }
            window->dirty = true;
        }
        ImGui::EndDisabled();
    }
    else
    {
        ImGui::Dummy(ImVec2(button_width(reset), ImGui::GetFrameHeight()));   // keeps the columns still
    }
    ImGui::PopID();
}

// The controller mapped: picked by the user, else port 1's, else the keyboard.
static uint32_t mapped_controller(InputWindow* window, const InputWindowContext* context)
{
    InputSystem* input = context->input;
    if (window->mapping < input->controllers.size())
    {
        return window->mapping;
    }
    if (input->ports[0] >= 0)
    {
        window->mapping = static_cast<uint32_t>(input->ports[0]);
        return window->mapping;
    }
    window->mapping = 0;
    for (uint32_t c = 0; c < input->controllers.size(); c++)
    {
        if (is_keyboard(input, c))
        {
            window->mapping = c;
            break;
        }
    }
    return window->mapping;
}

static void draw_bindings(InputWindow* window, const InputWindowContext* context, InputAction first, InputAction last,
                          Str left_title, float label_width)
{
    InputSystem* input = context->input;
    const Localization* strings = context->strings;
    if (input->controllers.empty())
    {
        return;
    }
    uint32_t controller = mapped_controller(window, context);

    // MAPPING [ controller v ]
    char label[200] = {};
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(piccolo_text(strings, Str::input_mapping));
    ImGui::SameLine(label_width);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    controller_label(context, controller, label, sizeof(label));
    ImGui::BeginDisabled(window->capturing);
    if (retro_ui_begin_combo("##mapping", label))
    {
        for (uint32_t c = 0; c < input->controllers.size(); c++)
        {
            ImGui::PushID(static_cast<int>(c));
            controller_label(context, c, label, sizeof(label));
            if (ImGui::Selectable(label, c == controller))
            {
                window->mapping = c;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    controller = mapped_controller(window, context);
    InputBindingSet defaults = {};
    input_window_controller_defaults(input, controller, &defaults);

    // BIND ALL, DEFAULTS
    const InputDevice* device = input_devices_find_const(&input->devices, input->controllers[controller].device_id);
    ImGui::BeginDisabled(device == nullptr || !device->connected);
    if (retro_ui_button(piccolo_text(strings, Str::input_bind_all)))
    {
        window->bind_all = true;
        window->bind_all_last = last;
        start_capture(window, context, controller, first);
    }
    retro_ui_item_tooltip(piccolo_text(strings, Str::input_bind_all_help));
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::input_defaults)))
    {
        for (uint32_t a = static_cast<uint32_t>(first); a <= static_cast<uint32_t>(last); a++)
        {
            for (uint32_t s = 0; s < INPUT_SOURCES_PER_ACTION; s++)
            {
                input->controllers[controller].bindings.sources[a][s] = defaults.sources[a][s];
            }
        }
        window->dirty = true;
    }
    retro_ui_item_tooltip(piccolo_text(strings, Str::input_defaults_help));
    ImGui::EndDisabled();

    // GAME / GAMEPAD (or KEY), then the rows, which scroll under the titles.
    ImGui::TextDisabled("%s", piccolo_text(strings, left_title));
    ImGui::SameLine(label_width);
    ImGui::TextDisabled("%s", piccolo_text(strings, is_keyboard(input, controller) ? Str::input_column_key : Str::input_column_pad));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::BeginChild("rows", ImVec2(0.0f, 0.0f));
    for (uint32_t a = static_cast<uint32_t>(first); a <= static_cast<uint32_t>(last); a++)
    {
        binding_row(window, context, controller, &defaults, static_cast<InputAction>(a), label_width);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------
// DEVICES
// ---------------------------------------------------------------------------

static void draw_devices(InputWindow* window, const InputWindowContext* context, float label_width)
{
    InputSystem* input = context->input;
    const Localization* strings = context->strings;
    for (size_t d = 0; d < input->devices.devices.size(); d++)
    {
        InputDevice* device = &input->devices.devices[d];
        int32_t first = input_first_controller(input, device->id);
        ImGui::PushID(static_cast<int>(device->id));
        retro_ui_separator_text(device->name.c_str());
        if (!device->connected || input_devices_ignored(device))
        {
            Str state = !device->connected ? Str::input_device_disconnected
                        : device->ignore == InputIgnore::yes ? Str::input_device_ignored : Str::input_device_ignored_auto;
            ImGui::TextDisabled("%s", piccolo_text(strings, state));
        }

        if (device->kind == InputDeviceKind::gamepad && first >= 0)
        {
            InputController* controller = &input->controllers[static_cast<size_t>(first)];
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(piccolo_text(strings, Str::input_name));
            ImGui::SameLine(label_width);
            name_field(window, context, static_cast<uint32_t>(first), ImGui::GetContentRegionAvail().x);

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(piccolo_text(strings, Str::input_deadzone));
            retro_ui_item_tooltip(piccolo_text(strings, Str::input_deadzone_help));
            ImGui::SameLine(label_width);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            int percent = static_cast<int>(controller->deadzone * 100.0f + 0.5f);
            retro_ui_push_field_text();
            if (ImGui::SliderInt("##deadzone", &percent, 0, 50, "%d%%"))
            {
                controller->deadzone = static_cast<float>(percent) / 100.0f;
                window->dirty = true;
            }
            retro_ui_pop_field_text();

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(piccolo_text(strings, Str::input_ignore));
            retro_ui_item_tooltip(piccolo_text(strings, Str::input_ignore_help));
            ImGui::SameLine(label_width);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            static const Str CHOICES[] = {Str::input_ignore_auto, Str::input_ignore_yes, Str::input_ignore_no};
            static const InputIgnore VALUES[] = {InputIgnore::automatic, InputIgnore::yes, InputIgnore::no};
            if (retro_ui_begin_combo("##ignore", piccolo_text(strings, CHOICES[static_cast<int>(device->ignore)])))
            {
                for (int i = 0; i < 3; i++)
                {
                    if (ImGui::Selectable(piccolo_text(strings, CHOICES[i]), device->ignore == VALUES[i]))
                    {
                        device->ignore = VALUES[i];
                        window->dirty = true;
                    }
                }
                ImGui::EndCombo();
            }
        }

        if (device->kind == InputDeviceKind::keyboard)
        {
            // Its players: a NAME each; REMOVE for all but the first.
            const char* remove = piccolo_text(strings, Str::input_remove_part);
            uint32_t number = 0;
            for (uint32_t c = 0; c < input->controllers.size(); c++)
            {
                if (input->controllers[c].device_id != device->id)
                {
                    continue;
                }
                number++;
                ImGui::PushID(static_cast<int>(c));
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%s %u", piccolo_text(strings, Str::input_player), static_cast<unsigned>(number));
                ImGui::SameLine(label_width);
                bool removable = static_cast<int32_t>(c) != first;
                float width = ImGui::GetContentRegionAvail().x - (removable ? button_width(remove) + ImGui::GetStyle().ItemSpacing.x : 0.0f);
                name_field(window, context, c, width);
                if (removable)
                {
                    ImGui::SameLine();
                    if (retro_ui_button(remove))
                    {
                        input_remove_controller(input, c);
                        window->mapping = UINT32_MAX;   // indices moved
                        window->names.clear();
                        window->dirty = true;
                        ImGui::PopID();
                        break;
                    }
                }
                ImGui::PopID();
            }
            if (retro_ui_button(piccolo_text(strings, Str::input_add_part)))
            {
                char name[64] = {};
                const char* base = first >= 0 ? input->controllers[static_cast<size_t>(first)].name.c_str() : device->name.c_str();
                std::snprintf(name, sizeof(name), "%s %u", base, static_cast<unsigned>(number + 1));
                InputBindingSet empty = {};
                window->mapping = input_add_controller(input, device->id, name, &empty);   // ready to map
                window->dirty = true;
            }
            retro_ui_item_tooltip(piccolo_text(strings, Str::input_add_part_help));
        }
        ImGui::PopID();
    }
}

// ---------------------------------------------------------------------------
// The window
// ---------------------------------------------------------------------------

// The next action of a BIND ALL, or the end of it.
static void next_or_stop(InputWindow* window, const InputWindowContext* context)
{
    if (window->bind_all && window->capture_action != window->bind_all_last)
    {
        start_capture(window, context, window->capture_controller,
                      static_cast<InputAction>(static_cast<uint32_t>(window->capture_action) + 1));
        return;
    }
    window->bind_all = false;
    input_capture_cancel(context->input);
    window->capturing = false;
}

// A SET / BIND ALL in progress: the next key or button binds; Escape stops;
// 5 s without one skips (BIND ALL) or gives up (SET).
static void run_capture(InputWindow* window, const InputWindowContext* context)
{
    InputSystem* input = context->input;
    if (!window->capturing)
    {
        return;
    }
    if (window->capture_controller >= input->controllers.size() || !window->open)
    {
        window->bind_all = false;
        input_capture_cancel(input);
        window->capturing = false;
        return;
    }
    InputSource source = {};
    if (input_capture_poll(input, &source))
    {
        if (source.type == InputSourceType::key && source.code == HID_ESCAPE)
        {
            window->bind_all = false;
            window->capturing = false;
            return;
        }
        InputBindingSet* bindings = &input->controllers[window->capture_controller].bindings;
        input_unbind_all(bindings, window->capture_action);
        input_bind(bindings, window->capture_action, source);
        window->dirty = true;
        next_or_stop(window, context);
        return;
    }
    if (!input->capture.active || context->now_ms - window->capture_started_ms > CAPTURE_TIMEOUT_MS)
    {
        next_or_stop(window, context);
    }
}

static bool begin_tab(const InputWindow* window, const char* text, const char* id, int index, int target)
{
    char label[200] = {};
    (void)window;
    return ImGui::BeginTabItem(with_id(label, sizeof(label), text, id), nullptr,
                               target == index ? ImGuiTabItemFlags_SetSelected : 0);
}

static void begin_content()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::BeginChild("content", ImVec2(0.0f, 0.0f));
}

static void end_content()
{
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void input_window_draw(InputWindow* window, const InputWindowContext* context, ImFont* pixel_font, int32_t scale)
{
    run_capture(window, context);
    if (!window->open)
    {
        return;
    }
    const Localization* strings = context->strings;
    float block = static_cast<float>(scale);
    const ImGuiIO& io = ImGui::GetIO();
    retro_ui_push_theme(pixel_font, scale);
    ImVec2 size(360.0f * block, 240.0f * block);
    size.x = size.x < io.DisplaySize.x - 8.0f * block ? size.x : io.DisplaySize.x - 8.0f * block;
    size.y = size.y < io.DisplaySize.y - 24.0f * block ? size.y : io.DisplaySize.y - 24.0f * block;
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    char label[200] = {};
    if (ImGui::Begin(with_id(label, sizeof(label), piccolo_text(strings, Str::input_title), "input"), nullptr, flags))
    {
        retro_ui_begin_window(scale);
        if (retro_ui_close_button())
        {
            window->open = false;
        }
        float label_width = 110.0f * block;
        if (ImGui::BeginTabBar("input_tabs"))
        {
            int target = window->capturing ? -1 : retro_ui_tab_switch(window->tab, 5);
            if (begin_tab(window, piccolo_text(strings, Str::input_tab_settings), "settings", 0, target))
            {
                window->tab = 0;
                begin_content();
                draw_settings(window, context);
                end_content();
                ImGui::EndTabItem();
            }
            if (begin_tab(window, piccolo_text(strings, Str::input_tab_retropad), "retropad", 1, target))
            {
                window->tab = 1;
                draw_bindings(window, context, InputAction::pad_b, InputAction::pad_right_y_minus, Str::input_column_game,
                              label_width);
                ImGui::EndTabItem();
            }
            if (begin_tab(window, piccolo_text(strings, Str::input_section_interface), "interface", 2, target))
            {
                window->tab = 2;
                draw_bindings(window, context, InputAction::ui_up, InputAction::ui_open_menu, Str::input_column_action,
                              label_width);
                ImGui::EndTabItem();
            }
            if (begin_tab(window, piccolo_text(strings, Str::input_section_hotkeys), "hotkeys", 3, target))
            {
                window->tab = 3;
                // How HOTKEY ENABLE works (a setting, scoped), then the bindings.
                static const char* const HOTKEY_KEYS[] = {PICCOLO_INPUT_HOTKEY_MODE};
                draw_scoped_settings(window, context, HOTKEY_KEYS, 1);
                ImGui::Separator();
                draw_bindings(window, context, InputAction::hotkey_enable, InputAction::hotkey_fast_forward,
                              Str::input_column_action, label_width);
                ImGui::EndTabItem();
            }
            if (begin_tab(window, piccolo_text(strings, Str::input_tab_devices), "devices", 4, target))
            {
                window->tab = 4;
                begin_content();
                draw_devices(window, context, label_width);
                end_content();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        retro_ui_end_window();
    }
    ImGui::End();
    retro_ui_pop_theme();
}
