#include "input_window.h"

#include <SDL3/SDL.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "imgui.h"
#include "pad_picture.h"
#include "piccolo_text.h"
#include "pixel_font_imgui.h"
#include "piccolo_settings.h"
#include "retro_ui.h"
#include "settings_ui.h"

// SET gives up after this long without a key or button.
// BIND ALL waits this long before taking the next control, so one press
// doesn't land on two.
constexpr uint64_t BIND_ALL_COOL_OFF_MS = 500;
constexpr uint16_t HID_ESCAPE = 41;

// The label of each InputAction, in its order.
static const Str ACTION_LABELS[INPUT_ACTION_COUNT] = {
    Str::input_action_pad_b, Str::input_action_pad_y, Str::input_action_pad_select, Str::input_action_pad_start,
    Str::input_action_pad_up, Str::input_action_pad_down, Str::input_action_pad_left, Str::input_action_pad_right,
    Str::input_action_pad_a, Str::input_action_pad_x, Str::input_action_pad_l, Str::input_action_pad_r,
    Str::input_action_pad_l2, Str::input_action_pad_r2, Str::input_action_pad_l3, Str::input_action_pad_r3,
    Str::input_action_left_x_plus, Str::input_action_left_x_minus, Str::input_action_left_y_plus,
    Str::input_action_left_y_minus, Str::input_action_right_x_plus, Str::input_action_right_x_minus,
    Str::input_action_right_y_plus, Str::input_action_right_y_minus, Str::input_action_pad_turbo,
    Str::input_action_cursor_right, Str::input_action_cursor_left, Str::input_action_cursor_down, Str::input_action_cursor_up,
    Str::input_action_mouse_left, Str::input_action_mouse_right, Str::input_action_mouse_middle,
    Str::input_action_mouse_wheel_up, Str::input_action_mouse_wheel_down, Str::input_action_gun_reload,
    Str::input_action_ui_up, Str::input_action_ui_down, Str::input_action_ui_left, Str::input_action_ui_right,
    Str::input_action_ui_accept, Str::input_action_ui_back, Str::input_action_ui_page_left,
    Str::input_action_ui_page_right, Str::input_action_ui_open_menu,
    Str::input_action_hotkey_enable, Str::input_action_hotkey_menu, Str::input_action_hotkey_quick_menu,
    Str::input_action_hotkey_save_state, Str::input_action_hotkey_load_state, Str::input_action_hotkey_fast_forward,
    Str::input_action_hotkey_grab_mouse,
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

// The layout control behind a source, for its label (count = none).
static InputMapControl layout_control(InputSource source)
{
    if (source.type == InputSourceType::button)
    {
        switch (static_cast<InputButton>(source.code))
        {
        case InputButton::south:          return InputMapControl::south;
        case InputButton::east:           return InputMapControl::east;
        case InputButton::west:           return InputMapControl::west;
        case InputButton::north:          return InputMapControl::north;
        case InputButton::back:           return InputMapControl::back;
        case InputButton::guide:          return InputMapControl::guide;
        case InputButton::start:          return InputMapControl::start;
        case InputButton::left_stick:     return InputMapControl::left_stick;
        case InputButton::right_stick:    return InputMapControl::right_stick;
        case InputButton::left_shoulder:  return InputMapControl::left_shoulder;
        case InputButton::right_shoulder: return InputMapControl::right_shoulder;
        default:                          return InputMapControl::count;
        }
    }
    if (source.type == InputSourceType::axis_plus && source.code == static_cast<uint16_t>(InputAxis::left_trigger))
    {
        return InputMapControl::left_trigger;
    }
    if (source.type == InputSourceType::axis_plus && source.code == static_cast<uint16_t>(InputAxis::right_trigger))
    {
        return InputMapControl::right_trigger;
    }
    return InputMapControl::count;
}

const InputLayout* input_window_layout(const InputSystem* input, const InputLayouts* layouts, const InputDevice* device)
{
    if (layouts == nullptr || device == nullptr || device->kind != InputDeviceKind::gamepad)
    {
        return nullptr;
    }
    const InputProfile* profile = input_profile(input, device->guid);
    return profile != nullptr ? input_layouts_find(layouts, profile->appearance.c_str()) : nullptr;
}

void input_window_source_label(const InputSdl* sdl, const InputDevice* device, const InputLayout* layout,
                               const InputProfile* profile, InputSource source, char* out, size_t out_size)
{
    if (profile != nullptr && source.type == InputSourceType::button && source.code < INPUT_BUTTON_COUNT &&
        !profile->button_labels[source.code].empty())
    {
        std::snprintf(out, out_size, "%s", profile->button_labels[source.code].c_str());   // a name given to it
        return;
    }
    SDL_Gamepad* gamepad = device != nullptr ? input_sdl_gamepad(sdl, device->id) : nullptr;
    InputMapControl control = layout_control(source);
    if (layout != nullptr && control != InputMapControl::count && !layout->labels[static_cast<uint32_t>(control)].empty())
    {
        std::snprintf(out, out_size, "%s", layout->labels[static_cast<uint32_t>(control)].c_str());
        return;
    }
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
        case InputButton::misc1:          text = "MISC 1"; break;
        case InputButton::right_paddle1:  text = "RIGHT PADDLE 1"; break;
        case InputButton::left_paddle1:   text = "LEFT PADDLE 1"; break;
        case InputButton::right_paddle2:  text = "RIGHT PADDLE 2"; break;
        case InputButton::left_paddle2:   text = "LEFT PADDLE 2"; break;
        case InputButton::touchpad:       text = "TOUCHPAD"; break;
        case InputButton::misc2:          text = "MISC 2"; break;
        case InputButton::misc3:          text = "MISC 3"; break;
        case InputButton::misc4:          text = "MISC 4"; break;
        case InputButton::misc5:          text = "MISC 5"; break;   // MAP CONTROLLER names them L4 / R4 when they are
        case InputButton::misc6:          text = "MISC 6"; break;   // the extra shoulders (the profile's labels)
        case InputButton::count:          break;
        }
        break;
    case InputSourceType::mouse:
        switch (static_cast<InputMouseButton>(source.code))
        {
        case InputMouseButton::left:       text = "MOUSE LEFT"; break;
        case InputMouseButton::right:      text = "MOUSE RIGHT"; break;
        case InputMouseButton::middle:     text = "MOUSE MIDDLE"; break;
        case InputMouseButton::x1:         text = "MOUSE BACK"; break;
        case InputMouseButton::x2:         text = "MOUSE FORWARD"; break;
        case InputMouseButton::wheel_up:   text = "WHEEL UP"; break;
        case InputMouseButton::wheel_down: text = "WHEEL DOWN"; break;
        case InputMouseButton::count:      break;
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
        input_default_bindings_for(device, set);   // more keyboard players start with no keys
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static float button_width(const char* label)
{
    return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
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
    static const char* const KEYS[] = {PICCOLO_INPUT_SHARE_PORT1, PICCOLO_INPUT_PRESS_TIME, PICCOLO_INPUT_HOLD_TIME};
    draw_scoped_settings(window, context, KEYS, 3);
}

// ---------------------------------------------------------------------------
// INTERFACE / HOTKEYS
// ---------------------------------------------------------------------------

static void start_capture(InputWindow* window, const InputWindowContext* context, uint32_t controller, InputAction action,
                          uint32_t row)
{
    if (!window->capturing)
    {
        window->capture_backup = context->input->controllers[controller].bindings;   // a new pass: CANCEL goes back to this
    }
    window->capturing = true;
    window->capture_action = action;
    window->capture_row = row;
    window->capture_controller = controller;
    window->capture_started_ms = context->now_ms;
    input_capture_begin(context->input, context->input->controllers[controller].device_id);
}

void input_window_draw_sources(const InputSdl* sdl, const InputDevice* device, const InputLayout* layout,
                               const InputProfile* profile, const InputSource* sources, float width, const Localization* strings)
{
    char names[INPUT_SOURCES_PER_ACTION][48] = {};
    uint32_t count = 0;
    for (uint32_t s = 0; s < INPUT_SOURCES_PER_ACTION; s++)
    {
        if (sources[s].type != InputSourceType::none)
        {
            input_window_source_label(sdl, device, layout, profile, sources[s], names[count], sizeof(names[count]));
            count++;
        }
    }
    if (count == 0)
    {
        ImGui::TextDisabled("%s", piccolo_text(strings, Str::input_none));
        return;
    }
    std::string all;
    for (uint32_t s = 0; s < count; s++)
    {
        all += s == 0 ? "" : ", ";
        all += names[s];
    }
    if (ImGui::CalcTextSize(all.c_str()).x <= width)
    {
        ImGui::TextUnformatted(all.c_str());
        return;
    }
    if (count == 1)
    {
        // One that doesn't fit: cut short with "…" (whole characters), all of it in the tooltip.
        const char* ellipsis = "\xE2\x80\xA6";
        std::string cut = all;
        while (!cut.empty() && ImGui::CalcTextSize((cut + ellipsis).c_str()).x > width)
        {
            cut.pop_back();
            while (!cut.empty() && (static_cast<unsigned char>(cut.back()) & 0xC0u) == 0x80u)
            {
                cut.pop_back();   // the rest of a UTF-8 character
            }
        }
        ImGui::TextUnformatted((cut + ellipsis).c_str());
        retro_ui_item_tooltip(all.c_str());
        return;
    }
    // Doesn't fit: the first one and how many more; all of them in the tooltip.
    char shortened[80] = {};
    std::snprintf(shortened, sizeof(shortened), "%s +%u", names[0], static_cast<unsigned>(count - 1));
    ImGui::TextUnformatted(shortened);
    retro_ui_item_tooltip(all.c_str());
}

void input_window_draw_waiting(bool keyboard, uint64_t seconds_left, float hold_progress, const Localization* strings)
{
    char text[100] = {};
    std::snprintf(text, sizeof(text), piccolo_text(strings, keyboard ? Str::input_press_key : Str::input_press_button),
                  static_cast<int>(seconds_left));
    retro_ui_push_section_text();
    ImGui::TextUnformatted(text);
    retro_ui_pop_section_text();
    // A gamepad's control is taken once held: a ring closes next to it.
    char spinner[4] = {};
    pixel_font_imgui_spinner(hold_progress, spinner);
    if (spinner[0] != '\0')
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 220.0f / 255.0f, 80.0f / 255.0f, 1.0f), "%s", spinner);   // yellow, like the lit pad
    }
}

// One action: a row per source (its label on the first only), each with
// SET and CLEAR; the first also has + (one more source for it: a new row)
// and RESET when it differs from the defaults.
static void binding_row(InputWindow* window, const InputWindowContext* context, uint32_t controller_index,
                        const InputBindingSet* defaults, InputAction action, float label_width)
{
    InputSystem* input = context->input;
    InputController* controller = &input->controllers[controller_index];
    const InputDevice* device = input_devices_find_const(&input->devices, controller->device_id);
    const InputLayout* layout = input_window_layout(input, context->layouts, device);
    const Localization* strings = context->strings;
    uint32_t index = static_cast<uint32_t>(action);
    InputSource* sources = controller->bindings.sources[index];
    bool changed = !same_sources(sources, defaults->sources[index]);
    bool waiting = window->capturing && window->capture_action == action && window->capture_controller == controller_index;
    uint32_t count = input_sources_count(sources);
    uint32_t rows = count > 0 ? count : 1;   // an action with nothing still has its row
    if (waiting && window->capture_row >= rows)
    {
        rows = window->capture_row + 1;      // + : the new row, waiting
    }

    const char* set = piccolo_text(strings, Str::input_set);
    const char* clear = piccolo_text(strings, Str::input_clear);
    const char* reset = piccolo_text(strings, Str::input_reset);
    const char* add = "+";
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float row_buttons = button_width(set) + button_width(clear) + spacing;
    float first_buttons = row_buttons + button_width(add) + spacing + (changed ? button_width(reset) + spacing : 0.0f);

    ImGui::PushID(static_cast<int>(index));
    for (uint32_t r = 0; r < rows; r++)
    {
        ImGui::PushID(static_cast<int>(r));
        ImGui::AlignTextToFramePadding();
        if (r == 0)
        {
            if (waiting)
            {
                retro_ui_push_section_text();   // the one being bound stands out
                ImGui::TextUnformatted(piccolo_text(strings, ACTION_LABELS[index]));
                retro_ui_pop_section_text();
            }
            else if (changed)
            {
                // Changed from the defaults: highlighted, like an override.
                ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive), "%s", piccolo_text(strings, ACTION_LABELS[index]));
            }
            else
            {
                ImGui::TextUnformatted(piccolo_text(strings, ACTION_LABELS[index]));
            }
        }
        else
        {
            ImGui::TextUnformatted("");   // more sources of the same action: no label
        }

        // The buttons at the right end; the binding gets the room before them.
        float buttons_x = ImGui::GetWindowContentRegionMax().x - (r == 0 ? first_buttons : row_buttons);
        ImGui::SameLine(label_width);
        if (waiting && window->capture_row == r)
        {
            uint64_t elapsed = context->now_ms - window->capture_started_ms;
            uint64_t total = context->input->settings.press_ms + (window->bind_all ? BIND_ALL_COOL_OFF_MS : 0);
            uint64_t left = elapsed < total ? (total - elapsed + 999) / 1000 : 0;
            input_window_draw_waiting(device != nullptr && device->kind == InputDeviceKind::keyboard, left,
                                      input_capture_hold_progress(input, context->now_ms), strings);
            ImGui::SetScrollHereY(0.5f);   // BIND ALL: keep it in view
        }
        else
        {
            InputSource one[INPUT_SOURCES_PER_ACTION] = {};
            one[0] = r < count ? sources[r] : InputSource{};
            input_window_draw_sources(context->sdl, device, layout,
                                      input_profile(input, input_controller_profile_id(input, controller_index)), one,
                                      buttons_x - ImGui::GetCursorPosX() - spacing, strings);
        }
        ImGui::SameLine(buttons_x > ImGui::GetCursorPosX() ? buttons_x : 0.0f);
        bool usable = !window->capturing && device != nullptr && device->connected;
        if (r == 0)
        {
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
                ImGui::SameLine();
            }
            ImGui::BeginDisabled(!usable || count == 0 || count >= INPUT_SOURCES_PER_ACTION);
            if (retro_ui_button(add))
            {
                window->bind_all = false;
                start_capture(window, context, controller_index, action, count);   // a new row
            }
            retro_ui_item_tooltip(piccolo_text(strings, Str::input_add_help));
            ImGui::EndDisabled();
            ImGui::SameLine();
        }
        ImGui::BeginDisabled(!usable);
        if (retro_ui_button(set))
        {
            window->bind_all = false;
            start_capture(window, context, controller_index, action, r < count ? r : count);
        }
        // The floating pad shows the row under the mouse, or the one in focus
        // while navigating with a gamepad or keys.
        if (ImGui::IsItemHovered() || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible))
        {
            window->pad_action = action;   // shown on the next frame
            window->pad_row = r;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(r >= count);
        if (retro_ui_button(clear))
        {
            input_sources_remove(sources, r);
            window->dirty = true;
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        ImGui::PopID();
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
    if (!input->ports[0].empty())
    {
        window->mapping = input->ports[0][0];
        return window->mapping;
    }
    window->mapping = 0;
    for (uint32_t c = 0; c < input->controllers.size(); c++)
    {
        if (is_keyboard(input, c))   // the keyboard is never ignored
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
            const InputDevice* candidate = input_devices_find_const(&input->devices, input->controllers[c].device_id);
            if (candidate != nullptr && input_devices_ignored(candidate))
            {
                continue;   // ignored devices aren't mapped
            }
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

    // HOTKEYS: how this controller's modifier works.
    if (first == InputAction::hotkey_enable)
    {
        static const Str MODE_NAMES[] = {Str::hotkey_mode_hold, Str::hotkey_mode_one, Str::hotkey_mode_stays};
        static const InputHotkeyMode MODES[] = {InputHotkeyMode::hold, InputHotkeyMode::one_hotkey, InputHotkeyMode::stays_on};
        InputController* mapped = &input->controllers[controller];
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(piccolo_text(strings, Str::input_hotkey_mode));
        retro_ui_item_tooltip(piccolo_text(strings, Str::input_hotkey_mode_help));
        ImGui::SameLine(label_width);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (retro_ui_begin_combo("##hotkey_mode", piccolo_text(strings, MODE_NAMES[static_cast<int>(mapped->hotkey_mode)])))
        {
            for (int m = 0; m < 3; m++)
            {
                if (ImGui::Selectable(piccolo_text(strings, MODE_NAMES[m]), mapped->hotkey_mode == MODES[m]))
                {
                    mapped->hotkey_mode = MODES[m];
                    window->dirty = true;
                }
            }
            ImGui::EndCombo();
        }
    }

    // BIND ALL, DEFAULTS
    const InputDevice* device = input_devices_find_const(&input->devices, input->controllers[controller].device_id);
    ImGui::BeginDisabled(device == nullptr || !device->connected);
    if (retro_ui_button(piccolo_text(strings, Str::input_bind_all)))
    {
        window->bind_all = true;
        window->bind_all_last = last;
        start_capture(window, context, controller, first, 0);
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
    // CANCEL: stops a SET / BIND ALL and puts back what it changed.
    ImGui::SameLine();
    ImGui::BeginDisabled(!window->capturing || window->capture_controller != controller);
    if (retro_ui_button(piccolo_text(strings, Str::input_cancel_binding)))
    {
        input->controllers[controller].bindings = window->capture_backup;
        input_capture_cancel(input);
        window->capturing = false;
        window->bind_all = false;
    }
    retro_ui_item_tooltip(piccolo_text(strings, Str::input_cancel_binding_help));
    ImGui::EndDisabled();

    // GAME / GAMEPAD (or KEY), then the rows, which scroll under the titles.
    ImGui::TextDisabled("%s", piccolo_text(strings, left_title));
    ImGui::SameLine(label_width);
    ImGui::TextDisabled("%s", piccolo_text(strings, is_keyboard(input, controller) ? Str::input_column_key : Str::input_column_pad));
    // A gamepad's pad floats in the screen's corner: the control being bound
    // while it is held, else where the row in focus is now.
    InputMapControl lit = InputMapControl::count;
    bool minus = false;
    InputSource held = {};
    if (window->capturing && window->capture_controller == controller && input_capture_candidate(input, &held))
    {
        lit = input_map_control_for_source(held);
        minus = held.type == InputSourceType::axis_minus;
    }
    else
    {
        InputAction shown = window->capturing ? window->capture_action : window->pad_action;
        uint32_t row = window->capturing ? window->capture_row : window->pad_row;
        if (static_cast<uint32_t>(shown) < INPUT_ACTION_COUNT && row < INPUT_SOURCES_PER_ACTION)
        {
            InputSource source = input->controllers[controller].bindings.sources[static_cast<uint32_t>(shown)][row];
            lit = input_map_control_for_source(source);
            minus = source.type == InputSourceType::axis_minus;
        }
    }
    window->pad_action = InputAction::count;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::BeginChild("rows", ImVec2(0.0f, 0.0f));
    for (uint32_t a = static_cast<uint32_t>(first); a <= static_cast<uint32_t>(last); a++)
    {
        binding_row(window, context, controller, &defaults, static_cast<InputAction>(a), label_width);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    if (device != nullptr && device->kind == InputDeviceKind::gamepad)
    {
        float block = ImGui::GetFontSize() / PIXEL_FONT_IMGUI_UNIT;
        pad_picture_draw_floating(input_window_layout(input, context->layouts, device), lit, minus, 1.25f * block);
    }
}

// ---------------------------------------------------------------------------
// DEVICES
// ---------------------------------------------------------------------------

static void draw_device(InputWindow* window, const InputWindowContext* context, InputDevice* device, float label_width,
                        bool open_by_default);

static void draw_devices(InputWindow* window, const InputWindowContext* context, float label_width)
{
    InputSystem* input = context->input;
    const Localization* strings = context->strings;
    // Controllers SDL doesn't know as gamepads: unusable until mapped.
    if (!context->sdl->unmapped.empty())
    {
        retro_ui_separator_text(piccolo_text(strings, Str::input_map_unmapped));
        for (size_t u = 0; u < context->sdl->unmapped.size(); u++)
        {
            uint32_t joystick = context->sdl->unmapped[u];
            ImGui::PushID(static_cast<int>(10000 + joystick));
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(input_sdl_joystick_name(joystick).c_str());
            const char* map = piccolo_text(strings, Str::input_map);
            float x = ImGui::GetWindowContentRegionMax().x - button_width(map);
            ImGui::SameLine(x > ImGui::GetCursorPosX() ? x : 0.0f);
            if (retro_ui_button(map))
            {
                window->map_request = joystick;
            }
            ImGui::PopID();
        }
    }
    // ENABLED (connected and used) open; DETECTED (ignored, or not
    // connected now) folded.
    for (int section = 0; section < 2; section++)
    {
        retro_ui_separator_text(piccolo_text(strings, section == 0 ? Str::input_section_enabled : Str::input_section_detected));
        for (size_t d = 0; d < input->devices.devices.size(); d++)
        {
            InputDevice* candidate = &input->devices.devices[d];
            bool enabled = candidate->connected && !input_devices_ignored(candidate);
            if (enabled != (section == 0))
            {
                continue;
            }
            draw_device(window, context, candidate, label_width, enabled);
        }
    }
}

// The extra buttons this gamepad has (MISC, paddles, touchpad: what MAP
// CONTROLLER's STEP 4 adds), each with a name of the user's own ("VOLUME
// UP"), kept in its profile and shown wherever its buttons are listed.
static void extra_button_names(InputWindow* window, const InputWindowContext* context, const InputDevice* device,
                               uint32_t controller, float label_width)
{
    static const InputButton EXTRAS[] = {
        InputButton::misc1, InputButton::misc2, InputButton::misc3, InputButton::misc4, InputButton::misc5,
        InputButton::misc6, InputButton::right_paddle1, InputButton::left_paddle1, InputButton::right_paddle2,
        InputButton::left_paddle2, InputButton::touchpad,
    };
    InputSystem* input = context->input;
    SDL_Gamepad* gamepad = input_sdl_gamepad(context->sdl, device->id);
    if (gamepad == nullptr)
    {
        return;
    }
    std::string id = input_controller_profile_id(input, controller);
    const InputLayout* layout = input_window_layout(input, context->layouts, device);
    bool any = false;
    for (size_t e = 0; e < sizeof(EXTRAS) / sizeof(EXTRAS[0]); e++)
    {
        if (!SDL_GamepadHasButton(gamepad, static_cast<SDL_GamepadButton>(EXTRAS[e])))
        {
            continue;
        }
        if (!any)
        {
            ImGui::TextUnformatted(piccolo_text(context->strings, Str::input_extra_names));
            retro_ui_item_tooltip(piccolo_text(context->strings, Str::input_extra_names_help));
            any = true;
        }
        InputSource source = {};
        source.type = InputSourceType::button;
        source.code = static_cast<uint16_t>(EXTRAS[e]);
        char own[48] = {};
        input_window_source_label(context->sdl, device, layout, nullptr, source, own, sizeof(own));   // its own name
        ImGui::PushID(static_cast<int>(EXTRAS[e]));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(own);
        ImGui::SameLine(label_width);
        const InputProfile* profile = input_profile(input, id);
        char buffer[48] = {};
        std::snprintf(buffer, sizeof(buffer), "%s", profile != nullptr ? profile->button_labels[source.code].c_str() : "");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        retro_ui_push_field_text();
        if (ImGui::InputText("##label", buffer, sizeof(buffer)))
        {
            input_profile_for(input, id)->button_labels[source.code] = buffer;
            window->dirty = true;
        }
        retro_ui_pop_field_text();
        ImGui::PopID();
    }
}

// SDL's mapping fields as the sources they give ("a" = the bottom button...).
struct SdlField
{
    const char*     name;
    InputSourceType type;
    uint16_t        code;
};

static const SdlField SDL_FIELDS[] = {
    {"a", InputSourceType::button, static_cast<uint16_t>(InputButton::south)},
    {"b", InputSourceType::button, static_cast<uint16_t>(InputButton::east)},
    {"x", InputSourceType::button, static_cast<uint16_t>(InputButton::west)},
    {"y", InputSourceType::button, static_cast<uint16_t>(InputButton::north)},
    {"back", InputSourceType::button, static_cast<uint16_t>(InputButton::back)},
    {"guide", InputSourceType::button, static_cast<uint16_t>(InputButton::guide)},
    {"start", InputSourceType::button, static_cast<uint16_t>(InputButton::start)},
    {"leftstick", InputSourceType::button, static_cast<uint16_t>(InputButton::left_stick)},
    {"rightstick", InputSourceType::button, static_cast<uint16_t>(InputButton::right_stick)},
    {"leftshoulder", InputSourceType::button, static_cast<uint16_t>(InputButton::left_shoulder)},
    {"rightshoulder", InputSourceType::button, static_cast<uint16_t>(InputButton::right_shoulder)},
    {"dpup", InputSourceType::button, static_cast<uint16_t>(InputButton::dpad_up)},
    {"dpdown", InputSourceType::button, static_cast<uint16_t>(InputButton::dpad_down)},
    {"dpleft", InputSourceType::button, static_cast<uint16_t>(InputButton::dpad_left)},
    {"dpright", InputSourceType::button, static_cast<uint16_t>(InputButton::dpad_right)},
    {"misc1", InputSourceType::button, static_cast<uint16_t>(InputButton::misc1)},
    {"paddle1", InputSourceType::button, static_cast<uint16_t>(InputButton::right_paddle1)},
    {"paddle2", InputSourceType::button, static_cast<uint16_t>(InputButton::left_paddle1)},
    {"paddle3", InputSourceType::button, static_cast<uint16_t>(InputButton::right_paddle2)},
    {"paddle4", InputSourceType::button, static_cast<uint16_t>(InputButton::left_paddle2)},
    {"touchpad", InputSourceType::button, static_cast<uint16_t>(InputButton::touchpad)},
    {"misc2", InputSourceType::button, static_cast<uint16_t>(InputButton::misc2)},
    {"misc3", InputSourceType::button, static_cast<uint16_t>(InputButton::misc3)},
    {"misc4", InputSourceType::button, static_cast<uint16_t>(InputButton::misc4)},
    {"misc5", InputSourceType::button, static_cast<uint16_t>(InputButton::misc5)},
    {"misc6", InputSourceType::button, static_cast<uint16_t>(InputButton::misc6)},
    {"lefttrigger", InputSourceType::axis_plus, static_cast<uint16_t>(InputAxis::left_trigger)},
    {"righttrigger", InputSourceType::axis_plus, static_cast<uint16_t>(InputAxis::right_trigger)},
};

// A stick axis field's name ("leftx" = LEFT STICK X), or nullptr.
static const char* stick_axis_name(const std::string& field)
{
    if (field == "leftx") return "LEFT STICK X";
    if (field == "lefty") return "LEFT STICK Y";
    if (field == "rightx") return "RIGHT STICK X";
    if (field == "righty") return "RIGHT STICK Y";
    return nullptr;
}

// A raw joystick element in words: "b3" BUTTON 3, "h0.1" HAT 0 UP, "+a2"
// AXIS 2 +, "a5~" AXIS 5 (INVERTED). Anything else as it is.
static std::string element_words(const std::string& element)
{
    char text[64] = {};
    if (element.size() >= 2 && element[0] == 'b')
    {
        std::snprintf(text, sizeof(text), "BUTTON %s", element.c_str() + 1);
        return text;
    }
    if (element.size() >= 4 && element[0] == 'h')
    {
        size_t dot = element.find('.');
        if (dot != std::string::npos)
        {
            int mask = std::atoi(element.c_str() + dot + 1);
            const char* way = mask == 1 ? "UP" : mask == 2 ? "RIGHT" : mask == 4 ? "DOWN" : mask == 8 ? "LEFT" : "?";
            std::snprintf(text, sizeof(text), "HAT %s %s", element.substr(1, dot - 1).c_str(), way);
            return text;
        }
    }
    std::string axis = element;
    const char* half = "";
    if (!axis.empty() && (axis[0] == '+' || axis[0] == '-'))
    {
        half = axis[0] == '+' ? " +" : " -";
        axis = axis.substr(1);
    }
    bool inverted = !axis.empty() && axis[axis.size() - 1] == '~';
    if (inverted)
    {
        axis = axis.substr(0, axis.size() - 1);
    }
    if (axis.size() >= 2 && axis[0] == 'a')
    {
        std::snprintf(text, sizeof(text), "AXIS %s%s%s", axis.c_str() + 1, half, inverted ? " (INVERTED)" : "");
        return text;
    }
    return element;
}

// The SDL mapping this gamepad uses now, read only: each control by the
// name the rest of the window gives it, and the joystick's own input behind
// it. (To check what MAP CONTROLLER made, or that its CANCEL put it back.)
static void sdl_mapping_view(const InputWindowContext* context, const InputDevice* device, uint32_t controller, float label_width)
{
    SDL_Gamepad* gamepad = input_sdl_gamepad(context->sdl, device->id);
    if (gamepad == nullptr)
    {
        return;
    }
    char* mapping = SDL_GetGamepadMapping(gamepad);
    if (mapping == nullptr)
    {
        return;
    }
    InputSystem* input = context->input;
    const InputLayout* layout = input_window_layout(input, context->layouts, device);
    const InputProfile* profile = input_profile(input, input_controller_profile_id(input, controller));
    if (retro_ui_collapsing_header(piccolo_text(context->strings, Str::input_sdl_mapping), 0))
    {
        // "<guid>,<name>,field:value,...": past the GUID and the name.
        const char* field = mapping;
        for (int commas = 0; commas < 2 && field != nullptr; commas++)
        {
            field = std::strchr(field, ',');
            field = field != nullptr ? field + 1 : nullptr;
        }
        while (field != nullptr && *field != '\0')
        {
            const char* end = std::strchr(field, ',');
            std::string text = end != nullptr ? std::string(field, static_cast<size_t>(end - field)) : std::string(field);
            field = end != nullptr ? end + 1 : nullptr;
            size_t colon = text.find(':');
            if (colon == std::string::npos)
            {
                continue;
            }
            std::string name = text.substr(0, colon);
            std::string value = text.substr(colon + 1);
            if (name == "platform" || name == "crc" || name == "hint" || name == "type")
            {
                continue;   // about the mapping, not a control
            }
            char label[64] = {};
            const char* stick = stick_axis_name(name);
            bool known = stick != nullptr;
            if (stick != nullptr)
            {
                std::snprintf(label, sizeof(label), "%s", stick);
            }
            for (size_t f = 0; !known && f < sizeof(SDL_FIELDS) / sizeof(SDL_FIELDS[0]); f++)
            {
                if (name == SDL_FIELDS[f].name)
                {
                    InputSource source = {};
                    source.type = SDL_FIELDS[f].type;
                    source.code = SDL_FIELDS[f].code;
                    input_window_source_label(context->sdl, device, layout, profile, source, label, sizeof(label));
                    known = true;
                }
            }
            if (!known)
            {
                std::snprintf(label, sizeof(label), "%s", name.c_str());   // a field piccolo doesn't know: as SDL names it
            }
            ImGui::TextUnformatted(label);
            ImGui::SameLine(label_width);
            ImGui::TextDisabled("%s", element_words(value).c_str());
        }
    }
    SDL_free(mapping);
}

static void draw_device(InputWindow* window, const InputWindowContext* context, InputDevice* device, float label_width,
                        bool open_by_default)
{
    InputSystem* input = context->input;
    const Localization* strings = context->strings;
    {
        int32_t first = input_first_controller(input, device->id);
        ImGui::PushID(static_cast<int>(device->id));
        char header[200] = {};
        std::snprintf(header, sizeof(header), "%s###device", device->name.c_str());
        if (!retro_ui_collapsing_header(header, open_by_default ? ImGuiTreeNodeFlags_DefaultOpen : 0))
        {
            ImGui::PopID();
            return;
        }
        if (!device->connected || input_devices_ignored(device))
        {
            Str state = !device->connected ? Str::input_device_disconnected
                        : device->ignore == InputIgnore::yes ? Str::input_device_ignored : Str::input_device_ignored_auto;
            ImGui::TextDisabled("%s", piccolo_text(strings, state));
        }

        if (device->kind == InputDeviceKind::gamepad && first >= 0)
        {
            InputController* controller = &input->controllers[static_cast<size_t>(first)];
            uint32_t joystick = input_sdl_joystick_of(context->sdl, device->id);
            if (joystick != 0)
            {
                // Map it again as it is (its buttons by position, its appearance).
                if (retro_ui_button(piccolo_text(strings, Str::input_map)))
                {
                    window->map_request = joystick;
                }
            }
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
            extra_button_names(window, context, device, static_cast<uint32_t>(first), label_width);
            sdl_mapping_view(context, device, static_cast<uint32_t>(first), label_width);
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
                      static_cast<InputAction>(static_cast<uint32_t>(window->capture_action) + 1), 0);
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
    uint64_t since = context->now_ms - window->capture_started_ms;
    InputSource source = {};
    if ((!window->bind_all || since >= BIND_ALL_COOL_OFF_MS) && input_capture_poll(input, context->now_ms, &source))
    {
        if (source.type == InputSourceType::key && source.code == HID_ESCAPE)
        {
            window->bind_all = false;
            window->capturing = false;
            return;
        }
        InputBindingSet* bindings = &input->controllers[window->capture_controller].bindings;
        if (window->bind_all)
        {
            input_unbind_all(bindings, window->capture_action);   // BIND ALL: this one, alone
            input_bind(bindings, window->capture_action, source);
        }
        else
        {
            input_sources_set(bindings->sources[static_cast<uint32_t>(window->capture_action)], window->capture_row, source);
        }
        window->dirty = true;
        next_or_stop(window, context);
        return;
    }
    InputSource held = {};
    bool holding = input_capture_candidate(input, &held);   // no time-out while a control is being held
    if (!input->capture.active || (!holding && since > context->input->settings.press_ms + (window->bind_all ? BIND_ALL_COOL_OFF_MS : 0)))
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
            int target = window->capturing ? -1 : retro_ui_tab_switch(window->tab, 4);
            if (begin_tab(window, piccolo_text(strings, Str::input_tab_settings), "settings", 0, target))
            {
                window->tab = 0;
                begin_content();
                draw_settings(window, context);
                end_content();
                ImGui::EndTabItem();
            }
            if (begin_tab(window, piccolo_text(strings, Str::input_section_interface), "interface", 1, target))
            {
                window->tab = 1;
                draw_bindings(window, context, InputAction::ui_up, InputAction::ui_open_menu, Str::input_column_action,
                              label_width);
                ImGui::EndTabItem();
            }
            if (begin_tab(window, piccolo_text(strings, Str::input_section_hotkeys), "hotkeys", 2, target))
            {
                window->tab = 2;
                draw_bindings(window, context, InputAction::hotkey_enable, InputAction::hotkey_grab_mouse,
                              Str::input_column_action, label_width);
                ImGui::EndTabItem();
            }
            if (begin_tab(window, piccolo_text(strings, Str::input_tab_devices), "devices", 3, target))
            {
                window->tab = 3;
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
