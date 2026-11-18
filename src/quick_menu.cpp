#include "quick_menu.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "imgui.h"

#include "app_shell.h"
#include "debug_ui.h"
#include "input_window.h"
#include "pad_picture.h"
#include "piccolo_settings.h"
#include "piccolo_text.h"
#include "retro_ui.h"

// Room for any thumbnail (the picture at x1: a few hundred pixels across).
constexpr uint32_t THUMBNAIL_CAPACITY = 1024;

// The used slots' button color (0xAARRGGBB), so they stand out from empty ones.
constexpr uint32_t USED_SLOT = 0xFF3A6EA5u;

static ImVec4 color_of(uint32_t argb)
{
    return ImVec4(static_cast<float>((argb >> 16) & 0xFFu) / 255.0f, static_cast<float>((argb >> 8) & 0xFFu) / 255.0f,
                  static_cast<float>(argb & 0xFFu) / 255.0f, static_cast<float>(argb >> 24) / 255.0f);
}

void quick_menu_open_controls(QuickMenu* menu, uint32_t slot)
{
    quick_menu_open(menu, slot);
    menu->open_on_tab = 2;
}

void quick_menu_open_remap(QuickMenu* menu, uint32_t slot, uint32_t port, uint32_t controller)
{
    quick_menu_open_controls(menu, slot);
    menu->remap = true;
    menu->remap_port = port;
    menu->remap_controller = controller;
}

void quick_menu_open(QuickMenu* menu, uint32_t slot)
{
    menu->open_on_tab = menu->tab;   // back on the tab it was closed on
    menu->remap = false;
    menu->capturing = false;
    menu->bind_all = false;
    menu->pad_action = InputAction::count;
    menu->combo_editing = false;
    menu->macro_editing = false;
    menu->open = true;
    menu->slot = slot >= LIBRETRO_STATE_SLOT_FIRST && slot <= LIBRETRO_STATE_SLOT_LAST ? slot : LIBRETRO_STATE_SLOT_FIRST;
    menu->refresh = true;
}

void quick_menu_refresh(QuickMenu* menu)
{
    menu->refresh = true;
}

// Loads the selected slot's PNG into `pixels` (B8G8R8A8, opaque) when it isn't the one shown.
static void load_thumbnail(QuickMenu* menu)
{
    const StateSlotInfo* info = &menu->slots[menu->slot];
    if (!info->used || !info->has_thumbnail)
    {
        menu->has_picture = false;
        menu->shown_slot = 0;
        return;
    }
    if (menu->shown_slot == menu->slot && menu->shown_modified == info->modified)
    {
        return;
    }
    menu->shown_slot = menu->slot;
    menu->shown_modified = info->modified;
    menu->has_picture = false;
    SDL_Surface* png = SDL_LoadPNG(info->thumbnail_path);
    SDL_Surface* converted = png != nullptr ? SDL_ConvertSurface(png, SDL_PIXELFORMAT_ARGB8888) : nullptr;
    if (converted != nullptr && converted->w > 0 && converted->h > 0 &&
        static_cast<uint32_t>(converted->w) <= THUMBNAIL_CAPACITY && static_cast<uint32_t>(converted->h) <= THUMBNAIL_CAPACITY)
    {
        menu->width = static_cast<uint32_t>(converted->w);
        menu->height = static_cast<uint32_t>(converted->h);
        menu->pixels.resize(static_cast<size_t>(menu->width) * menu->height);
        for (uint32_t y = 0; y < menu->height; y++)
        {
            const uint8_t* row = static_cast<const uint8_t*>(converted->pixels) + static_cast<size_t>(y) * static_cast<size_t>(converted->pitch);
            std::memcpy(&menu->pixels[static_cast<size_t>(y) * menu->width], row, static_cast<size_t>(menu->width) * 4);
        }
        menu->upload_pending = true;
    }
    else
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Quick menu: could not read thumbnail %s: %s", info->thumbnail_path,
                    SDL_GetError());
    }
    SDL_DestroySurface(converted);
    SDL_DestroySurface(png);
}

void quick_menu_upload(QuickMenu* menu, AppShell* shell)
{
    if (!menu->upload_pending)
    {
        return;
    }
    if (menu->texture.image == VK_NULL_HANDLE)
    {
        if (!streaming_texture_create(&menu->texture, &shell->vulkan, THUMBNAIL_CAPACITY, THUMBNAIL_CAPACITY))
        {
            menu->upload_pending = false;
            return;
        }
        menu->texture_id = debug_ui_add_texture(menu->texture.view);
    }
    if (!app_shell_acquire(shell))
    {
        return;   // try on the next frame
    }
    if (streaming_texture_upload(&menu->texture, renderer_command_buffer(&shell->renderer),
                                 renderer_frame_index(&shell->renderer), menu->pixels.data(), menu->width, menu->height))
    {
        menu->has_picture = true;
    }
    menu->upload_pending = false;
}

void quick_menu_destroy(QuickMenu* menu, AppShell* shell)
{
    if (menu->texture.image != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(shell->vulkan.device);
        if (menu->texture_id != 0)
        {
            debug_ui_remove_texture(menu->texture_id);
        }
        streaming_texture_destroy(&menu->texture, &shell->vulkan);
    }
    menu->texture_id = 0;
}

// The nine slot buttons; the focused one (arrows) or the clicked one is the selection.
static void draw_slots(QuickMenu* menu, float block, bool appearing)
{
    float size = 18.0f * block;
    for (uint32_t slot = LIBRETRO_STATE_SLOT_FIRST; slot <= LIBRETRO_STATE_SLOT_LAST; slot++)
    {
        if (slot > LIBRETRO_STATE_SLOT_FIRST)
        {
            ImGui::SameLine();
        }
        bool used = menu->slots[slot].used;
        bool selected = slot == menu->slot;
        if (used)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, color_of(USED_SLOT));
        }
        if (selected)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, block);
        }
        // The focused button is the selection (arrows move it), so a slot
        // picked otherwise (opening, 1-9) takes the focus too.
        if ((appearing || menu->focus_slot) && selected)
        {
            ImGui::SetKeyboardFocusHere();
            menu->focus_slot = false;
        }
        char label[16] = {};
        std::snprintf(label, sizeof(label), "%u", slot);
        if (retro_ui_button_sized(label, size, size) || ImGui::IsItemFocused())
        {
            menu->slot = slot;
        }
        if (selected)
        {
            ImGui::PopStyleVar();
        }
        if (used)
        {
            ImGui::PopStyleColor();
        }
    }
    // Number keys pick a slot directly.
    for (uint32_t slot = LIBRETRO_STATE_SLOT_FIRST; slot <= LIBRETRO_STATE_SLOT_LAST; slot++)
    {
        ImGuiKey key = static_cast<ImGuiKey>(static_cast<int>(ImGuiKey_1) + static_cast<int>(slot - 1));
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(key, false))
        {
            menu->slot = slot;
            menu->focus_slot = true;
        }
    }
}

// The SAVESTATES tab: slots, the selected one's thumbnail and date, SAVE /
// LOAD, RESET and CLOSE GAME.
static QuickMenuAction draw_states_tab(QuickMenu* menu, const QuickMenuContext* context, float block, bool appearing)
{
    const Localization* strings = context->strings;
    const ImGuiIO& io = ImGui::GetIO();
    QuickMenuAction action = QuickMenuAction::none;
    draw_slots(menu, block, appearing);
    load_thumbnail(menu);

    // The thumbnail at the biggest whole-number scale that fits (sharp pixels:
    // nearest filtering, on whole pixels), the slot's details beside it. Room:
    // most of the width, the height left under the slots.
    const StateSlotInfo* info = &menu->slots[menu->slot];
    ImVec2 room = ImGui::GetContentRegionAvail();
    room.x = static_cast<float>(static_cast<int>(room.x * 0.6f));
    room.y = static_cast<float>(static_cast<int>(room.y - ImGui::GetStyle().ItemSpacing.y));
    room.x = room.x > 160.0f * block ? room.x : 160.0f * block;
    room.y = room.y > 120.0f * block ? room.y : 120.0f * block;
    bool showing = menu->has_picture && menu->shown_slot == menu->slot && menu->texture_id != 0 && menu->width > 0 &&
                   menu->height > 0;
    ImVec2 box(room.x, room.x * 3.0f / 4.0f);   // no picture: a 4:3 box
    if (box.y > room.y)
    {
        box = ImVec2(room.y * 4.0f / 3.0f, room.y);
    }
    if (showing)
    {
        float width = static_cast<float>(menu->width);
        float height = static_cast<float>(menu->height);
        int scale_x = static_cast<int>(room.x / width);
        int scale_y = static_cast<int>(room.y / height);
        int scale = scale_x < scale_y ? scale_x : scale_y;
        if (scale >= 1)
        {
            box = ImVec2(width * static_cast<float>(scale), height * static_cast<float>(scale));
        }
        else
        {
            // Not even once: shrunk to fit (it can't be whole pixels then).
            float aspect = width / height;
            box = ImVec2(room.x, room.x / aspect);
            if (box.y > room.y)
            {
                box = ImVec2(room.y * aspect, room.y);
            }
        }
    }
    box = ImVec2(static_cast<float>(static_cast<int>(box.x)), static_cast<float>(static_cast<int>(box.y)));
    ImVec2 corner = ImGui::GetCursorScreenPos();
    corner = ImVec2(static_cast<float>(static_cast<int>(corner.x)), static_cast<float>(static_cast<int>(corner.y)));
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(corner, ImVec2(corner.x + box.x, corner.y + box.y), IM_COL32(0, 0, 0, 255));
    if (showing)
    {
        float u = 0.0f;
        float v = 0.0f;
        streaming_texture_uv_max(&menu->texture, &u, &v);
        const ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        draw_list->AddCallback(platform_io.DrawCallback_SetSamplerNearest, nullptr);
        draw_list->AddImage(ImTextureRef(static_cast<ImTextureID>(menu->texture_id)), corner,
                            ImVec2(corner.x + box.x, corner.y + box.y), ImVec2(0.0f, 0.0f), ImVec2(u, v));
        draw_list->AddCallback(platform_io.DrawCallback_SetSamplerLinear, nullptr);
    }
    ImGui::Dummy(box);
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text("%s %u", piccolo_text(strings, Str::quick_menu_slot), menu->slot);
    if (info->used)
    {
        SDL_DateTime date = {};
        SDL_TimeToDateTime(info->modified, &date, true);
        ImGui::Text("%04d-%02d-%02d %02d:%02d", date.year, date.month, date.day, date.hour, date.minute);
    }
    else
    {
        ImGui::TextDisabled("%s", piccolo_text(strings, Str::quick_menu_empty));
    }
    ImGui::Spacing();
    bool keys = !io.WantTextInput && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (retro_ui_button(piccolo_text(strings, Str::quick_menu_save)) || (keys && ImGui::IsKeyPressed(ImGuiKey_S, false)))
    {
        action = QuickMenuAction::save;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!info->used);
    if (retro_ui_button(piccolo_text(strings, Str::quick_menu_load)) ||
        (info->used && keys && ImGui::IsKeyPressed(ImGuiKey_L, false)))
    {
        action = QuickMenuAction::load;
    }
    ImGui::EndDisabled();
    ImGui::Spacing();
    if (retro_ui_button(piccolo_text(strings, Str::quick_menu_reset)))
    {
        action = QuickMenuAction::reset;
    }
    if (retro_ui_button(piccolo_text(strings, Str::quick_menu_close_game)))
    {
        action = QuickMenuAction::close_game;
    }
    ImGui::EndGroup();
    ImGui::Spacing();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", piccolo_text(strings, Str::quick_menu_keys));
    ImGui::PopTextWrapPos();
    return action;
}

// ---------------------------------------------------------------------------
// CONTROLS: remap one port's controller for this core and device
// ---------------------------------------------------------------------------

constexpr uint64_t REMAP_COOL_OFF_MS = 500;   // BIND ALL: before taking the next control
constexpr uint16_t HID_ESCAPE_KEY = 41;

// RetroPad button names when the core gives none (RETRO_DEVICE_ID_JOYPAD order).
static const Str PAD_LABELS[INPUT_PAD_BUTTON_COUNT] = {
    Str::input_action_pad_b, Str::input_action_pad_y, Str::input_action_pad_select, Str::input_action_pad_start,
    Str::input_action_pad_up, Str::input_action_pad_down, Str::input_action_pad_left, Str::input_action_pad_right,
    Str::input_action_pad_a, Str::input_action_pad_x, Str::input_action_pad_l, Str::input_action_pad_r,
    Str::input_action_pad_l2, Str::input_action_pad_r2, Str::input_action_pad_l3, Str::input_action_pad_r3,
};

// The RetroPad's stick directions, from pad_left_x_plus on.
static const Str STICK_LABELS[8] = {
    Str::input_action_left_x_plus,  Str::input_action_left_x_minus,  Str::input_action_left_y_plus,
    Str::input_action_left_y_minus, Str::input_action_right_x_plus, Str::input_action_right_x_minus,
    Str::input_action_right_y_plus, Str::input_action_right_y_minus,
};

struct RemapControl
{
    InputAction action;
    std::string label;   // the game's name for it (a stick's with the direction), or the RetroPad's
};

// The controls of the port's device, by the game's names: its joypad
// descriptors (each button once) and analog ones (each stick axis as its two
// directions: libretro's +Y is down), or the whole RetroPad if it sends none.
static void remap_controls(const QuickMenuContext* context, uint32_t port, std::vector<RemapControl>* controls)
{
    controls->clear();
    const LibretroHost* host = context->host;
    const Localization* strings = context->strings;
    bool seen[INPUT_PAD_BUTTON_COUNT] = {};
    bool seen_axis[2][2] = {};   // [left / right stick][x / y]
    uint32_t first_stick = static_cast<uint32_t>(InputAction::pad_left_x_plus);
    for (uint32_t d = 0; host != nullptr && d < host->input_info.descriptor_count; d++)
    {
        const LibretroInputDescriptor* descriptor = &host->input_info.descriptors[d];
        if (descriptor->port != port)
        {
            continue;
        }
        unsigned device = descriptor->device & RETRO_DEVICE_MASK;
        if (device == RETRO_DEVICE_JOYPAD && descriptor->id < INPUT_PAD_BUTTON_COUNT && !seen[descriptor->id])
        {
            seen[descriptor->id] = true;
            RemapControl control = {static_cast<InputAction>(descriptor->id), descriptor->description};
            controls->push_back(control);
        }
        else if (device == RETRO_DEVICE_ANALOG && descriptor->index <= RETRO_DEVICE_INDEX_ANALOG_RIGHT &&
                 descriptor->id <= RETRO_DEVICE_ID_ANALOG_Y && !seen_axis[descriptor->index][descriptor->id])
        {
            seen_axis[descriptor->index][descriptor->id] = true;
            bool x = descriptor->id == RETRO_DEVICE_ID_ANALOG_X;
            uint32_t plus = first_stick + descriptor->index * 4 + descriptor->id * 2;
            RemapControl positive = {static_cast<InputAction>(plus),
                                     std::string(descriptor->description) + " " +
                                         piccolo_text(strings, x ? Str::input_action_pad_right : Str::input_action_pad_down)};
            RemapControl negative = {static_cast<InputAction>(plus + 1),
                                     std::string(descriptor->description) + " " +
                                         piccolo_text(strings, x ? Str::input_action_pad_left : Str::input_action_pad_up)};
            controls->push_back(positive);
            controls->push_back(negative);
        }
    }
    if (controls->empty())
    {
        for (uint32_t b = 0; b < INPUT_PAD_BUTTON_COUNT; b++)
        {
            RemapControl control = {static_cast<InputAction>(b), piccolo_text(strings, PAD_LABELS[b])};
            controls->push_back(control);
        }
        for (uint32_t s = 0; s < 8; s++)
        {
            RemapControl control = {static_cast<InputAction>(first_stick + s), piccolo_text(strings, STICK_LABELS[s])};
            controls->push_back(control);
        }
    }
}

// The remap being edited: the game's when it has its own, else the core's.
static InputCoreProfile* remap_profile(QuickMenu* menu, const QuickMenuContext* context)
{
    InputSystem* input = context->input;
    std::string id = input_controller_profile_id(input, menu->remap_controller);
    uint32_t device = input->port_devices[menu->remap_port];
    bool game_has = !input->game_name.empty() &&
                    input_core_profile(input, input->core_name.c_str(), input->game_name.c_str(), device, id) != nullptr;
    return input_core_profile_for(input, input->core_name.c_str(), game_has ? input->game_name.c_str() : "", device, id);
}

// A combo's binding in the remap being edited (made if it has none).
static InputComboBinding* remap_combo_binding(QuickMenu* menu, const QuickMenuContext* context, const std::string& combo)
{
    InputCoreProfile* profile = remap_profile(menu, context);
    profile->decided = true;
    menu->input_dirty = true;
    for (size_t k = 0; k < profile->combos.size(); k++)
    {
        if (profile->combos[k].combo == combo)
        {
            return &profile->combos[k];
        }
    }
    InputComboBinding binding = {};
    binding.combo = combo;
    profile->combos.push_back(binding);
    return &profile->combos.back();
}

// A combo's binding in what plays now, or nullptr.
static const InputComboBinding* combo_binding_of(const InputCoreProfile* profile, const std::string& combo)
{
    for (size_t k = 0; profile != nullptr && k < profile->combos.size(); k++)
    {
        if (profile->combos[k].combo == combo)
        {
            return &profile->combos[k];
        }
    }
    return nullptr;
}

// The captured source becomes this action's binding for the core and device.
// This action's change for the core and device, made from what plays now
// when it has none (switching to custom keeps the rest as it was).
static InputBindingOverride* remap_override(QuickMenu* menu, const QuickMenuContext* context, InputAction action)
{
    InputBindingSet effective = {};
    input_effective_bindings(context->input, menu->remap_controller, menu->remap_port, &effective);
    InputCoreProfile* profile = remap_profile(menu, context);
    profile->decided = true;
    profile->custom = true;
    menu->input_dirty = true;
    for (size_t o = 0; o < profile->overrides.size(); o++)
    {
        if (profile->overrides[o].action == action)
        {
            return &profile->overrides[o];
        }
    }
    InputBindingOverride change = {};
    change.action = action;
    for (uint32_t s = 0; s < INPUT_SOURCES_PER_ACTION; s++)
    {
        change.sources[s] = effective.sources[static_cast<uint32_t>(action)][s];
    }
    profile->overrides.push_back(change);
    return &profile->overrides.back();
}

// The captured source on `row` (BIND ALL: alone).
static void remap_set(QuickMenu* menu, const QuickMenuContext* context, InputAction action, uint32_t row, InputSource source)
{
    if (!menu->capture_combo.empty())
    {
        input_sources_set(remap_combo_binding(menu, context, menu->capture_combo)->sources, row, source);
        return;
    }
    InputBindingOverride* change = remap_override(menu, context, action);
    if (menu->bind_all)
    {
        for (uint32_t s = 0; s < INPUT_SOURCES_PER_ACTION; s++)
        {
            change->sources[s] = InputSource{};
        }
        row = 0;
    }
    input_sources_set(change->sources, row, source);
}

static void remap_start_capture(QuickMenu* menu, const QuickMenuContext* context, InputAction action, uint32_t row)
{
    InputSystem* input = context->input;
    if (!menu->capturing)
    {
        menu->capture_backup = *remap_profile(menu, context);   // a new pass: CANCEL goes back to this
    }
    menu->capturing = true;
    menu->capture_action = action;
    menu->capture_combo.clear();
    menu->capture_row = row;
    menu->capture_started_ms = context->now_ms;
    menu->capture_seen = true;
    input_capture_begin(input, input->controllers[menu->remap_controller].device_id);
}

// SET / + on a combo's row.
static void remap_start_combo_capture(QuickMenu* menu, const QuickMenuContext* context, const std::string& combo, uint32_t row)
{
    menu->bind_all = false;
    remap_start_capture(menu, context, InputAction::count, row);
    menu->capture_combo = combo;
}

// The next control of a BIND ALL (in the list's order), or the end.
static void remap_next(QuickMenu* menu, const QuickMenuContext* context, const std::vector<RemapControl>& controls)
{
    if (menu->bind_all && menu->capture_combo.empty())
    {
        for (size_t i = 0; i + 1 < controls.size(); i++)
        {
            if (controls[i].action == menu->capture_action)
            {
                remap_start_capture(menu, context, controls[i + 1].action, 0);
                return;
            }
        }
    }
    menu->bind_all = false;
    menu->capturing = false;
    input_capture_cancel(context->input);
}

static void remap_run_capture(QuickMenu* menu, const QuickMenuContext* context, const std::vector<RemapControl>& controls)
{
    if (!menu->capturing)
    {
        return;
    }
    menu->capture_seen = true;
    uint64_t since = context->now_ms - menu->capture_started_ms;
    InputSource source = {};
    if ((!menu->bind_all || since >= REMAP_COOL_OFF_MS) && input_capture_poll(context->input, context->now_ms, &source))
    {
        if (source.type == InputSourceType::key && source.code == HID_ESCAPE_KEY)
        {
            menu->bind_all = false;
            menu->capturing = false;
            return;
        }
        remap_set(menu, context, menu->capture_action, menu->capture_row, source);
        remap_next(menu, context, controls);
        return;
    }
    InputSource held = {};
    bool holding = input_capture_candidate(context->input, &held);   // no time-out while a control is being held
    if (!context->input->capture.active ||
        (!holding && since > context->input->settings.press_ms + (menu->bind_all ? REMAP_COOL_OFF_MS : 0)))
    {
        remap_next(menu, context, controls);
    }
}

// CORE DEVICE n [ SNES Joypad v ]: what the core has on the port, from its
// own list (only shown when it lists more than one). Applied at once and
// kept per core.
static void draw_device_combo(const QuickMenuContext* context, uint32_t p, float label_width)
{
    LibretroHost* host = context->host;
    const Localization* strings = context->strings;
    if (host == nullptr || !host->game_loaded || p >= host->input_info.port_count)
    {
        return;
    }
    const LibretroPortControllers* types = &host->input_info.ports[p];
    if (types->count < 2)
    {
        return;   // nothing to choose
    }
    ImGui::PushID(static_cast<int>(100 + p));
    ImGui::AlignTextToFramePadding();
    char label[96] = {};
    std::snprintf(label, sizeof(label), "%s %u", piccolo_text(strings, Str::quick_menu_device), static_cast<unsigned>(p + 1));
    ImGui::TextUnformatted(label);
    // At least `label_width`, wider if the label (in this language) needs it.
    float needed = ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemSpacing.x * 2.0f + ImGui::GetStyle().WindowPadding.x;
    ImGui::SameLine(needed > label_width ? needed : label_width);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const char* current = libretro_input_controller_name(&host->input_info, p, host->port_devices[p]);
    if (retro_ui_begin_combo("##device", current != nullptr ? current : "?"))
    {
        for (uint32_t t = 0; t < types->count; t++)
        {
            ImGui::PushID(static_cast<int>(t));
            if (ImGui::Selectable(types->types[t].description, types->types[t].id == host->port_devices[p]))
            {
                libretro_host_set_port_device(host, p, types->types[t].id);
                if (context->settings != nullptr)
                {
                    settings_set_int(context->settings, SettingLayer::core, PICCOLO_INPUT_PORT_DEVICES[p],
                                     static_cast<int32_t>(types->types[t].id));
                }
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
}

// SCOPE [ THIS CORE (fbneo) v ]: the remap is the core's, or the game's own
// (THIS GAME starts as a copy of the core's; back to THIS CORE drops it).
static void draw_remap_scope(QuickMenu* menu, const QuickMenuContext* context, float label_width)
{
    InputSystem* input = context->input;
    const Localization* strings = context->strings;
    if (input->game_name.empty())
    {
        return;
    }
    std::string id = input_controller_profile_id(input, menu->remap_controller);
    uint32_t device = input->port_devices[menu->remap_port];
    const InputCoreProfile* game = input_core_profile(input, input->core_name.c_str(), input->game_name.c_str(), device, id);
    char core_label[300] = {};
    char game_label[600] = {};
    std::snprintf(core_label, sizeof(core_label), "%s (%s)", piccolo_text(strings, Str::scope_core), input->core_name.c_str());
    std::snprintf(game_label, sizeof(game_label), "%s (%s)", piccolo_text(strings, Str::scope_game), input->game_name.c_str());
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(piccolo_text(strings, Str::scope));
    retro_ui_item_tooltip(piccolo_text(strings, Str::remap_scope_help));
    ImGui::SameLine(label_width);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (retro_ui_begin_combo("##remap_scope", game != nullptr ? game_label : core_label))
    {
        if (ImGui::Selectable(core_label, game == nullptr) && game != nullptr)
        {
            input_core_profile_remove(input, game);
            menu->input_dirty = true;
        }
        if (ImGui::Selectable(game_label, game != nullptr) && game == nullptr)
        {
            const InputCoreProfile* core = input_core_profile(input, input->core_name.c_str(), "", device, id);
            InputCoreProfile copy = core != nullptr ? *core : InputCoreProfile{};
            copy.core = input->core_name;
            copy.game = input->game_name;
            copy.device = device;
            copy.profile_id = id;
            copy.decided = true;
            input->core_profiles.push_back(copy);
            menu->input_dirty = true;
        }
        ImGui::EndCombo();
    }
}

// The name of a RetroPad button in this game (its descriptor), else the RetroPad's.
static std::string button_label(uint32_t button, const std::vector<RemapControl>& controls, const Localization* strings)
{
    for (size_t i = 0; i < controls.size(); i++)
    {
        if (static_cast<uint32_t>(controls[i].action) == button)
        {
            return controls[i].label;
        }
    }
    return piccolo_text(strings, PAD_LABELS[button]);
}

// A combo as shown: its name, else its buttons ("A + B").
static std::string combo_label(const InputCombo* combo, const std::vector<RemapControl>& controls, const Localization* strings)
{
    if (!combo->name.empty())
    {
        return combo->name;
    }
    std::string text;
    for (uint32_t b = 0; b < INPUT_PAD_BUTTON_COUNT; b++)
    {
        if ((combo->buttons & (1u << b)) != 0)
        {
            text += text.empty() ? "" : " + ";
            text += button_label(b, controls, strings);
        }
    }
    return text;
}

// Making or changing a combo (in its modal): its NAME (optional) and its buttons.
static void draw_combo_editor(QuickMenu* menu, const QuickMenuContext* context, const std::vector<RemapControl>& controls,
                              float label_width)
{
    InputSystem* input = context->input;
    const Localization* strings = context->strings;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(piccolo_text(strings, Str::remap_combo_name));
    ImGui::SameLine(label_width);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    retro_ui_push_field_text();
    ImGui::InputTextWithHint("##combo_name", piccolo_text(strings, Str::remap_combo_name_hint), menu->combo_edit_name,
                             sizeof(menu->combo_edit_name));
    retro_ui_pop_field_text();
    // The buttons this game has (each once), else the whole RetroPad.
    bool any = false;
    for (size_t i = 0; i < controls.size(); i++)
    {
        any = any || static_cast<uint32_t>(controls[i].action) < INPUT_PAD_BUTTON_COUNT;
    }
    for (uint32_t b = 0; b < INPUT_PAD_BUTTON_COUNT; b++)
    {
        bool listed = !any;
        for (size_t i = 0; !listed && i < controls.size(); i++)
        {
            listed = static_cast<uint32_t>(controls[i].action) == b;
        }
        if (!listed)
        {
            continue;
        }
        ImGui::PushID(static_cast<int>(b));
        bool on = (menu->combo_edit_buttons & (1u << b)) != 0;
        if (retro_ui_checkbox(button_label(b, controls, strings).c_str(), &on))
        {
            menu->combo_edit_buttons = static_cast<uint16_t>(on ? (menu->combo_edit_buttons | (1u << b))
                                                                : (menu->combo_edit_buttons & ~(1u << b)));
        }
        ImGui::PopID();
    }
    ImGui::BeginDisabled(menu->combo_edit_buttons == 0);
    if (retro_ui_button(piccolo_text(strings, Str::remap_combo_save)))
    {
        std::string name = menu->combo_edit_name;
        size_t first = name.find_first_not_of(' ');
        size_t last = name.find_last_not_of(' ');
        name = first != std::string::npos ? name.substr(first, last - first + 1) : std::string();
        InputCombo* combo = nullptr;
        for (size_t c = 0; c < input->combos.size() && !menu->combo_edit_id.empty(); c++)
        {
            if (input->combos[c].core == input->core_name && input->combos[c].id == menu->combo_edit_id)
            {
                combo = &input->combos[c];
            }
        }
        if (combo == nullptr)
        {
            combo = input_combo_add(input, input->core_name.c_str(), name, menu->combo_edit_buttons);
        }
        combo->name = name;
        combo->buttons = menu->combo_edit_buttons;
        menu->combo_editing = false;
        menu->input_dirty = true;
    }
    ImGui::EndDisabled();
    if (!menu->combo_edit_id.empty())
    {
        ImGui::SameLine();
        if (retro_ui_button(piccolo_text(strings, Str::remap_combo_delete)))
        {
            input_combo_remove(input, input->core_name.c_str(), menu->combo_edit_id);
            menu->combo_editing = false;
            menu->input_dirty = true;
        }
        retro_ui_item_tooltip(piccolo_text(strings, Str::remap_combo_delete_help));
    }
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::map_cancel)))
    {
        menu->combo_editing = false;
    }
}

static void open_combo_editor(QuickMenu* menu, const InputCombo* combo)
{
    menu->combo_editing = true;
    menu->combo_edit_id = combo != nullptr ? combo->id : std::string();
    std::snprintf(menu->combo_edit_name, sizeof(menu->combo_edit_name), "%s", combo != nullptr ? combo->name.c_str() : "");
    menu->combo_edit_buttons = combo != nullptr ? combo->buttons : 0;
}

// The combo editor as a modal over the quick menu (B / Escape = CANCEL).
static void draw_combo_modal(QuickMenu* menu, const QuickMenuContext* context, const std::vector<RemapControl>& controls,
                             float label_width)
{
    char title[128] = {};
    std::snprintf(title, sizeof(title), "%s###combo_editor", piccolo_text(context->strings, Str::remap_combos));
    if (menu->combo_editing && !ImGui::IsPopupOpen(title))
    {
        ImGui::OpenPopup(title);
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(static_cast<int>(viewport->Size.x * 0.45f)), 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove))
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            menu->combo_editing = false;   // (B on a gamepad sends Escape while a popup is up)
        }
        else
        {
            draw_combo_editor(menu, context, controls, label_width);
        }
        if (!menu->combo_editing)
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// A macro as shown: its name, else its steps ("DOWN, DOWN + RIGHT, RIGHT + A").
static std::string macro_label(const InputMacro* macro, const Localization* strings)
{
    if (!macro->name.empty())
    {
        return macro->name;
    }
    std::string text;
    for (size_t s = 0; s < macro->steps.size(); s++)
    {
        if (macro->steps[s].buttons == 0)
        {
            continue;
        }
        std::string step;
        for (uint32_t b = 0; b < INPUT_PAD_BUTTON_COUNT; b++)
        {
            if ((macro->steps[s].buttons & (1u << b)) != 0)
            {
                step += step.empty() ? "" : " + ";
                step += piccolo_text(strings, PAD_LABELS[b]);
            }
        }
        text += text.empty() ? "" : ", ";
        text += step;
    }
    return text;
}

static void open_macro_editor(QuickMenu* menu, const InputMacro* macro)
{
    menu->macro_editing = true;
    menu->macro_edit_id = macro != nullptr ? macro->id : std::string();
    std::snprintf(menu->macro_edit_name, sizeof(menu->macro_edit_name), "%s", macro != nullptr ? macro->name.c_str() : "");
    menu->macro_edit_steps = macro != nullptr ? macro->steps : std::vector<InputMacroStep>();
    menu->macro_edit_repeat = macro != nullptr && macro->repeat;
    menu->macro_edit_selected = -1;
}

constexpr uint64_t MACRO_GET_READY_MS = 3000;
constexpr uint64_t MACRO_IDLE_STOP_MS = 1000;   // nothing held this long after a press: done
constexpr uint64_t MACRO_RECORD_MAX_MS = 10000;
constexpr uint16_t MACRO_FRAMES_MAX = 9999;

// The recording as steps (frames at the core's rate), replacing the macro's.
static void macro_record_finish(QuickMenu* menu, const QuickMenuContext* context, uint64_t now_ms)
{
    menu->macro_record = 0;
    context->input->suspended = false;
    std::vector<uint64_t>* times = &menu->macro_record_times;
    std::vector<uint16_t>* buttons = &menu->macro_record_buttons;
    if (times->empty())
    {
        return;   // nothing was pressed
    }
    times->push_back(now_ms);   // the end of the last change
    buttons->push_back(0);
    double fps = context->host != nullptr && context->host->av_info.timing.fps > 1.0 ? context->host->av_info.timing.fps : 60.0;
    std::vector<InputMacroStep> steps;
    for (size_t i = 0; i + 1 < times->size(); i++)
    {
        uint64_t from = static_cast<uint64_t>(static_cast<double>((*times)[i] - (*times)[0]) * fps / 1000.0 + 0.5);
        uint64_t to = static_cast<uint64_t>(static_cast<double>((*times)[i + 1] - (*times)[0]) * fps / 1000.0 + 0.5);
        if (to <= from)
        {
            continue;   // shorter than a frame
        }
        uint64_t frames = to - from;
        if (!steps.empty() && steps.back().buttons == (*buttons)[i])
        {
            frames += steps.back().frames;
            steps.pop_back();
        }
        InputMacroStep step = {};
        step.buttons = (*buttons)[i];
        step.frames = static_cast<uint16_t>(frames < MACRO_FRAMES_MAX ? frames : MACRO_FRAMES_MAX);
        steps.push_back(step);
    }
    while (!steps.empty() && steps.back().buttons == 0)
    {
        steps.pop_back();   // the wait at the end
    }
    menu->macro_edit_steps = steps;
    menu->macro_edit_selected = -1;
}

static void macro_record_cancel(QuickMenu* menu, const QuickMenuContext* context)
{
    if (menu->macro_record != 0)
    {
        menu->macro_record = 0;
        context->input->suspended = false;
    }
}

// The recorder, a frame: GET READY (the router suspended, so the presses
// only go here), then the controller's RetroPad changes until it rests.
static void macro_record_run(QuickMenu* menu, const QuickMenuContext* context)
{
    if (menu->macro_record == 0)
    {
        return;
    }
    menu->macro_record_seen = true;
    uint64_t now = context->now_ms;
    if (menu->macro_record == 1)
    {
        if (now - menu->macro_record_ms >= MACRO_GET_READY_MS)
        {
            menu->macro_record = 2;
            menu->macro_record_ms = now;
        }
        return;
    }
    uint16_t held = input_controller_pad_buttons(context->input, menu->remap_controller, menu->remap_port);
    uint16_t last = menu->macro_record_buttons.empty() ? 0 : menu->macro_record_buttons.back();
    if (held != last)
    {
        menu->macro_record_times.push_back(now);   // (the first press starts it)
        menu->macro_record_buttons.push_back(held);
        menu->macro_record_ms = now;
    }
    bool started = !menu->macro_record_times.empty();
    if ((started && held == 0 && now - menu->macro_record_ms >= MACRO_IDLE_STOP_MS) ||
        (started && now - menu->macro_record_times[0] >= MACRO_RECORD_MAX_MS) ||
        (!started && now - menu->macro_record_ms >= MACRO_RECORD_MAX_MS))
    {
        macro_record_finish(menu, context, held == 0 ? menu->macro_record_ms : now);
    }
}

// A frame count field (1 .. 9999).
static bool frames_field(const char* id, int32_t* value, float width)
{
    ImGui::SetNextItemWidth(width);
    retro_ui_push_field_text();
    bool changed = ImGui::InputInt(id, value, 1, 10);
    retro_ui_pop_field_text();
    *value = *value < 1 ? 1 : (*value > MACRO_FRAMES_MAX ? MACRO_FRAMES_MAX : *value);
    return changed;
}

// Making or changing a macro (in its modal): NAME, REPEAT WHILE HELD, its
// steps (frames, EDIT for its buttons, move, remove), + STEP, + PAUSE,
// RECORD, CHANGE STEPS / PAUSES TO.
static void draw_macro_editor(QuickMenu* menu, const QuickMenuContext* context, const std::vector<RemapControl>& controls,
                              float label_width)
{
    InputSystem* input = context->input;
    const Localization* strings = context->strings;
    std::vector<InputMacroStep>* steps = &menu->macro_edit_steps;
    float frames_width = ImGui::CalcTextSize("9999").x + ImGui::GetFrameHeight() * 2.0f + ImGui::GetStyle().FramePadding.x * 4.0f;
    macro_record_run(menu, context);
    if (menu->macro_record != 0)
    {
        // Recording: only what it's doing, and STOP.
        char text[160] = {};
        if (menu->macro_record == 1)
        {
            uint64_t left = (MACRO_GET_READY_MS - (context->now_ms - menu->macro_record_ms) + 999) / 1000;
            std::snprintf(text, sizeof(text), "%s %u", piccolo_text(strings, Str::remap_macro_get_ready), static_cast<unsigned>(left));
        }
        else
        {
            std::snprintf(text, sizeof(text), "%s (%s)", piccolo_text(strings, Str::remap_macro_recording),
                          input->controllers[menu->remap_controller].name.c_str());
        }
        retro_ui_push_section_text();
        ImGui::TextUnformatted(text);
        retro_ui_pop_section_text();
        if (retro_ui_button(piccolo_text(strings, Str::remap_macro_stop)))
        {
            if (menu->macro_record == 2)
            {
                macro_record_finish(menu, context, context->now_ms);
            }
            else
            {
                macro_record_cancel(menu, context);
            }
        }
        return;
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(piccolo_text(strings, Str::remap_combo_name));
    ImGui::SameLine(label_width);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    retro_ui_push_field_text();
    ImGui::InputTextWithHint("##macro_name", piccolo_text(strings, Str::remap_combo_name_hint), menu->macro_edit_name,
                             sizeof(menu->macro_edit_name));
    retro_ui_pop_field_text();
    retro_ui_checkbox(piccolo_text(strings, Str::remap_macro_repeat), &menu->macro_edit_repeat);
    retro_ui_item_tooltip(piccolo_text(strings, Str::remap_macro_repeat_help));

    retro_ui_separator_text(piccolo_text(strings, Str::remap_macro_steps));
    const char* edit = piccolo_text(strings, Str::remap_combo_edit);
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float padding = ImGui::GetStyle().FramePadding.x * 2.0f;
    float row_buttons = frames_width + ImGui::CalcTextSize(edit).x + ImGui::CalcTextSize("X").x + padding * 2.0f +
                        ImGui::GetFrameHeight() * 2.0f + spacing * 4.0f;
    int32_t remove = -1;
    int32_t move_from = -1;
    int32_t move_to = -1;
    for (size_t i = 0; i < steps->size(); i++)
    {
        InputMacroStep* step = &(*steps)[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::AlignTextToFramePadding();
        std::string text;
        for (uint32_t b = 0; b < INPUT_PAD_BUTTON_COUNT; b++)
        {
            if ((step->buttons & (1u << b)) != 0)
            {
                text += text.empty() ? "" : " + ";
                text += piccolo_text(strings, PAD_LABELS[b]);
            }
        }
        bool selected = menu->macro_edit_selected == static_cast<int32_t>(i);
        if (selected)
        {
            retro_ui_push_section_text();
        }
        ImGui::Text("%u  %s", static_cast<unsigned>(i + 1), text.empty() ? piccolo_text(strings, Str::remap_macro_pause) : text.c_str());
        if (selected)
        {
            retro_ui_pop_section_text();
        }
        float x = ImGui::GetWindowContentRegionMax().x - row_buttons;
        ImGui::SameLine(x > ImGui::GetCursorPosX() ? x : 0.0f);
        int32_t frames = step->frames;
        frames_field("##frames", &frames, frames_width);
        retro_ui_item_tooltip(piccolo_text(strings, Str::remap_macro_frames_help));
        step->frames = static_cast<uint16_t>(frames);
        ImGui::SameLine();
        if (retro_ui_button(edit))
        {
            menu->macro_edit_selected = selected ? -1 : static_cast<int32_t>(i);
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(i == 0);
        if (ImGui::ArrowButton("##up", ImGuiDir_Up))
        {
            move_from = static_cast<int32_t>(i);
            move_to = static_cast<int32_t>(i) - 1;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(i + 1 >= steps->size());
        if (ImGui::ArrowButton("##down", ImGuiDir_Down))
        {
            move_from = static_cast<int32_t>(i);
            move_to = static_cast<int32_t>(i) + 1;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (retro_ui_button("X"))
        {
            remove = static_cast<int32_t>(i);
        }
        if (selected)
        {
            // Its buttons (the game's names), indented under it.
            ImGui::Indent(label_width * 0.5f);
            for (uint32_t b = 0; b < INPUT_PAD_BUTTON_COUNT; b++)
            {
                ImGui::PushID(static_cast<int>(100 + b));
                bool on = (step->buttons & (1u << b)) != 0;
                if (retro_ui_checkbox(button_label(b, controls, strings).c_str(), &on))
                {
                    step->buttons = static_cast<uint16_t>(on ? (step->buttons | (1u << b)) : (step->buttons & ~(1u << b)));
                }
                ImGui::PopID();
            }
            ImGui::Unindent(label_width * 0.5f);
        }
        ImGui::PopID();
    }
    if (move_from >= 0)
    {
        InputMacroStep moved = (*steps)[static_cast<size_t>(move_from)];
        (*steps)[static_cast<size_t>(move_from)] = (*steps)[static_cast<size_t>(move_to)];
        (*steps)[static_cast<size_t>(move_to)] = moved;
        if (menu->macro_edit_selected == move_from)
        {
            menu->macro_edit_selected = move_to;
        }
        else if (menu->macro_edit_selected == move_to)
        {
            menu->macro_edit_selected = move_from;
        }
    }
    if (remove >= 0)
    {
        steps->erase(steps->begin() + remove);
        menu->macro_edit_selected = menu->macro_edit_selected == remove ? -1
                                    : menu->macro_edit_selected > remove ? menu->macro_edit_selected - 1
                                                                          : menu->macro_edit_selected;
    }
    if (retro_ui_button(piccolo_text(strings, Str::remap_macro_add_step)))
    {
        InputMacroStep step = {};
        step.frames = static_cast<uint16_t>(menu->macro_change_steps);
        steps->push_back(step);
        menu->macro_edit_selected = static_cast<int32_t>(steps->size() - 1);   // its buttons next
    }
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::remap_macro_add_pause)))
    {
        InputMacroStep pause = {};
        pause.frames = static_cast<uint16_t>(menu->macro_change_pauses);
        steps->push_back(pause);
    }
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::remap_macro_record)))
    {
        menu->macro_record = 1;
        menu->macro_record_ms = context->now_ms;
        menu->macro_record_seen = true;
        menu->macro_record_times.clear();
        menu->macro_record_buttons.clear();
        input->suspended = true;
    }
    retro_ui_item_tooltip(piccolo_text(strings, Str::remap_macro_record_help));

    // Every step, or every pause, to one length.
    const char* apply = piccolo_text(strings, Str::remap_macro_apply);
    for (int kind = 0; kind < 2; kind++)
    {
        ImGui::PushID(200 + kind);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(piccolo_text(strings, kind == 0 ? Str::remap_macro_change_steps : Str::remap_macro_change_pauses));
        ImGui::SameLine(label_width * 1.6f);
        int32_t* value = kind == 0 ? &menu->macro_change_steps : &menu->macro_change_pauses;
        frames_field("##to", value, frames_width);
        ImGui::SameLine();
        if (retro_ui_button(apply))
        {
            for (size_t i = 0; i < steps->size(); i++)
            {
                bool pause = (*steps)[i].buttons == 0;
                if (pause == (kind == 1))
                {
                    (*steps)[i].frames = static_cast<uint16_t>(*value);
                }
            }
        }
        ImGui::PopID();
    }

    ImGui::Spacing();
    bool playable = false;
    for (size_t i = 0; i < steps->size(); i++)
    {
        playable = playable || (*steps)[i].buttons != 0;
    }
    ImGui::BeginDisabled(!playable);
    if (retro_ui_button(piccolo_text(strings, Str::remap_combo_save)))
    {
        std::string name = menu->macro_edit_name;
        size_t first = name.find_first_not_of(' ');
        size_t last = name.find_last_not_of(' ');
        name = first != std::string::npos ? name.substr(first, last - first + 1) : std::string();
        InputMacro* macro = nullptr;
        for (size_t m = 0; m < input->macros.size() && !menu->macro_edit_id.empty(); m++)
        {
            if (input->macros[m].core == input->core_name && input->macros[m].id == menu->macro_edit_id)
            {
                macro = &input->macros[m];
            }
        }
        if (macro == nullptr)
        {
            macro = input_macro_add(input, input->core_name.c_str(), name);
        }
        macro->name = name;
        macro->steps = *steps;
        macro->repeat = menu->macro_edit_repeat;
        menu->macro_editing = false;
        menu->input_dirty = true;
    }
    ImGui::EndDisabled();
    if (!menu->macro_edit_id.empty())
    {
        ImGui::SameLine();
        if (retro_ui_button(piccolo_text(strings, Str::remap_combo_delete)))
        {
            input_macro_remove(input, input->core_name.c_str(), menu->macro_edit_id);
            menu->macro_editing = false;
            menu->input_dirty = true;
        }
        retro_ui_item_tooltip(piccolo_text(strings, Str::remap_macro_delete_help));
    }
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::map_cancel)))
    {
        menu->macro_editing = false;
    }
}

// The macro editor as a modal (B / Escape = CANCEL; while recording, it stops that only).
static void draw_macro_modal(QuickMenu* menu, const QuickMenuContext* context, const std::vector<RemapControl>& controls,
                             float label_width)
{
    char title[128] = {};
    std::snprintf(title, sizeof(title), "%s###macro_editor", piccolo_text(context->strings, Str::remap_macros));
    if (menu->macro_editing && !ImGui::IsPopupOpen(title))
    {
        ImGui::OpenPopup(title);
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(static_cast<int>(viewport->Size.x * 0.55f)), 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove))
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            if (menu->macro_record != 0)
            {
                macro_record_cancel(menu, context);
            }
            else
            {
                menu->macro_editing = false;
            }
        }
        else
        {
            draw_macro_editor(menu, context, controls, label_width);
        }
        if (!menu->macro_editing)
        {
            macro_record_cancel(menu, context);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// COMBOS / MACROS (`macros`): the core's, each bound to inputs in this
// remap like a control (+ / SET / CLEAR), EDIT to change it; + NEW.
static void draw_combo_rows(QuickMenu* menu, const QuickMenuContext* context, bool macros, const std::vector<RemapControl>& controls,
                            const InputDevice* pad, const InputLayout* pad_layout, const InputProfile* pad_profile,
                            float label_width)
{
    InputSystem* input = context->input;
    const Localization* strings = context->strings;
    ImGui::Spacing();
    retro_ui_separator_text(piccolo_text(strings, macros ? Str::remap_macros : Str::remap_combos));
    retro_ui_item_tooltip(piccolo_text(strings, macros ? Str::remap_macros_help : Str::remap_combos_help));
    const InputCoreProfile* active = input_active_core_profile(input, menu->remap_controller, menu->remap_port);
    const char* set = piccolo_text(strings, Str::input_set);
    const char* clear = piccolo_text(strings, Str::input_clear);
    const char* edit = piccolo_text(strings, Str::remap_combo_edit);
    const char* add = "+";
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float padding = ImGui::GetStyle().FramePadding.x * 2.0f;
    float row_buttons = ImGui::CalcTextSize(set).x + ImGui::CalcTextSize(clear).x + padding * 2.0f + spacing;
    float first_buttons = row_buttons + ImGui::CalcTextSize(add).x + ImGui::CalcTextSize(edit).x + padding * 2.0f + spacing * 2.0f;
    // (A copy of the ids: EDIT > DELETE changes the list.)
    std::vector<std::string> ids;
    for (size_t c = 0; !macros && c < input->combos.size(); c++)
    {
        if (input->combos[c].core == input->core_name)
        {
            ids.push_back(input->combos[c].id);
        }
    }
    for (size_t m = 0; macros && m < input->macros.size(); m++)
    {
        if (input->macros[m].core == input->core_name)
        {
            ids.push_back(input->macros[m].id);
        }
    }
    for (size_t c = 0; c < ids.size(); c++)
    {
        const InputCombo* combo = input_combo(input, input->core_name.c_str(), ids[c]);
        const InputMacro* macro = input_macro(input, input->core_name.c_str(), ids[c]);
        if (combo == nullptr && macro == nullptr)
        {
            continue;
        }
        const std::string id = ids[c];
        const InputComboBinding* binding = combo_binding_of(active, id);
        InputSource none[INPUT_SOURCES_PER_ACTION] = {};
        const InputSource* sources = binding != nullptr ? binding->sources : none;
        bool waiting = menu->capturing && menu->capture_combo == id;
        uint32_t count = input_sources_count(sources);
        uint32_t rows = count > 0 ? count : 1;
        if (waiting && menu->capture_row >= rows)
        {
            rows = menu->capture_row + 1;
        }
        std::string label = combo != nullptr ? combo_label(combo, controls, strings) : macro_label(macro, strings);
        ImGui::PushID(("combo/" + id).c_str());
        for (uint32_t r = 0; r < rows; r++)
        {
            ImGui::PushID(static_cast<int>(r));
            ImGui::AlignTextToFramePadding();
            if (waiting && r == 0)
            {
                retro_ui_push_section_text();
            }
            ImGui::TextUnformatted(r == 0 ? label.c_str() : "");
            if (waiting && r == 0)
            {
                retro_ui_pop_section_text();
            }
            float buttons_x = ImGui::GetWindowContentRegionMax().x - (r == 0 ? first_buttons : row_buttons);
            ImGui::SameLine(label_width);
            if (waiting && menu->capture_row == r)
            {
                uint64_t elapsed = context->now_ms - menu->capture_started_ms;
                uint64_t total = input->settings.press_ms;
                uint64_t left = elapsed < total ? (total - elapsed + 999) / 1000 : 0;
                input_window_draw_waiting(pad != nullptr && pad->kind == InputDeviceKind::keyboard, left,
                                          input_capture_hold_progress(input, context->now_ms), strings);
                ImGui::SetScrollHereY(0.5f);
            }
            else
            {
                InputSource one[INPUT_SOURCES_PER_ACTION] = {};
                one[0] = r < count ? sources[r] : InputSource{};
                input_window_draw_sources(context->sdl, pad, pad_layout, pad_profile, one,
                                          buttons_x - spacing - ImGui::GetCursorPosX(), strings);
            }
            ImGui::SameLine(buttons_x > ImGui::GetCursorPosX() ? buttons_x : 0.0f);
            ImGui::BeginDisabled(menu->capturing);
            if (r == 0)
            {
                if (retro_ui_button(edit))
                {
                    if (combo != nullptr)
                    {
                        open_combo_editor(menu, combo);
                    }
                    else
                    {
                        open_macro_editor(menu, macro);
                    }
                }
                ImGui::SameLine();
                ImGui::BeginDisabled(count == 0 || count >= INPUT_SOURCES_PER_ACTION);
                if (retro_ui_button(add))
                {
                    remap_start_combo_capture(menu, context, id, count);
                }
                retro_ui_item_tooltip(piccolo_text(strings, Str::input_add_help));
                ImGui::EndDisabled();
                ImGui::SameLine();
            }
            if (retro_ui_button(set))
            {
                remap_start_combo_capture(menu, context, id, r < count ? r : count);
            }
            if (ImGui::IsItemHovered() || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible))
            {
                menu->pad_combo = id;
                menu->pad_row = r;
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(r >= count);
            if (retro_ui_button(clear))
            {
                InputComboBinding* changed = remap_combo_binding(menu, context, id);
                input_sources_remove(changed->sources, r);
                if (input_sources_count(changed->sources) == 0)
                {
                    // Nothing on it any more: unbound.
                    InputCoreProfile* profile = remap_profile(menu, context);
                    for (size_t k = 0; k < profile->combos.size(); k++)
                    {
                        if (profile->combos[k].combo == id)
                        {
                            profile->combos.erase(profile->combos.begin() + static_cast<long>(k));
                            break;
                        }
                    }
                }
            }
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::PopID();
    }
    ImGui::BeginDisabled(menu->capturing);
    if (retro_ui_button(piccolo_text(strings, macros ? Str::remap_macro_new : Str::remap_combo_new)))
    {
        if (macros)
        {
            open_macro_editor(menu, nullptr);
        }
        else
        {
            open_combo_editor(menu, nullptr);
        }
    }
    ImGui::EndDisabled();
}

static void draw_remap(QuickMenu* menu, const QuickMenuContext* context, float block)
{
    InputSystem* input = context->input;
    const Localization* strings = context->strings;
    LibretroHost* host = context->host;
    if (host == nullptr || !host->game_loaded || menu->remap_controller >= input->controllers.size() ||
        menu->remap_port >= INPUT_PORT_COUNT)
    {
        menu->remap = false;
        return;
    }
    std::vector<RemapControl> controls;
    remap_controls(context, menu->remap_port, &controls);
    remap_run_capture(menu, context, controls);

    // <controller> -> <core device> MAPPING: what the controller is mapped to, not what it is.
    const char* device = libretro_input_controller_name(&host->input_info, menu->remap_port, host->port_devices[menu->remap_port]);
    char title[300] = {};
    std::snprintf(title, sizeof(title), piccolo_text(strings, Str::remap_title),
                  input->controllers[menu->remap_controller].name.c_str(), device != nullptr ? device : "RETROPAD");
    ImGui::BeginDisabled(menu->capturing);
    bool back = retro_ui_button(piccolo_text(strings, Str::map_back));
    ImGui::EndDisabled();
    if (back)
    {
        menu->remap = false;   // back to the ports (B on a gamepad does the same)
        menu->combo_editing = false;
        menu->macro_editing = false;
        return;
    }
    retro_ui_separator_text(title);
    ImGui::BeginDisabled(menu->capturing || menu->combo_editing);
    draw_remap_scope(menu, context, 100.0f * block);
    draw_device_combo(context, menu->remap_port, 100.0f * block);
    ImGui::EndDisabled();

    ImGui::BeginDisabled(menu->capturing);
    if (retro_ui_button(piccolo_text(strings, Str::input_bind_all)) && !controls.empty())
    {
        menu->bind_all = true;
        remap_start_capture(menu, context, controls[0].action, 0);
    }
    retro_ui_item_tooltip(piccolo_text(strings, Str::input_bind_all_help));
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::remap_use_retropad)))
    {
        InputCoreProfile* profile = remap_profile(menu, context);
        profile->decided = true;
        profile->custom = false;
        profile->overrides.clear();
        profile->combos.clear();
        menu->input_dirty = true;
    }
    retro_ui_item_tooltip(piccolo_text(strings, Str::remap_use_retropad_help));
    ImGui::SameLine();
    if (retro_ui_button(piccolo_text(strings, Str::remap_done)))
    {
        // Seen and accepted as it is: not asked again.
        remap_profile(menu, context)->decided = true;
        menu->input_dirty = true;
        menu->remap = false;
    }
    ImGui::EndDisabled();
    // CANCEL: stops a SET / BIND ALL and puts back what it changed.
    ImGui::SameLine();
    ImGui::BeginDisabled(!menu->capturing);
    if (retro_ui_button(piccolo_text(strings, Str::input_cancel_binding)))
    {
        *remap_profile(menu, context) = menu->capture_backup;
        input_capture_cancel(input);
        menu->capturing = false;
        menu->bind_all = false;
    }
    retro_ui_item_tooltip(piccolo_text(strings, Str::input_cancel_binding_help));
    ImGui::EndDisabled();

    InputBindingSet effective = {};
    input_effective_bindings(input, menu->remap_controller, menu->remap_port, &effective);
    const InputDevice* pad = input_devices_find_const(&input->devices, input->controllers[menu->remap_controller].device_id);
    // A gamepad's pad, floating in the screen's corner (the rows keep the
    // window's width), with where the control being bound (or the row in
    // focus) is now.
    bool show_pad = pad != nullptr && pad->kind == InputDeviceKind::gamepad;
    InputAction shown = menu->capturing ? menu->capture_action : menu->pad_action;
    std::string shown_combo = menu->capturing ? menu->capture_combo : menu->pad_combo;
    InputMapControl lit = InputMapControl::count;
    bool minus = false;
    uint32_t shown_index = static_cast<uint32_t>(shown);
    InputSource held = {};
    if (menu->capturing && input_capture_candidate(input, &held))
    {
        // Being held to be bound: that one (let go sooner and it isn't taken).
        lit = input_map_control_for_source(held);
        minus = held.type == InputSourceType::axis_minus;
    }
    uint32_t shown_row = menu->capturing ? menu->capture_row : menu->pad_row;
    if (lit == InputMapControl::count && !shown_combo.empty() && shown_row < INPUT_SOURCES_PER_ACTION)
    {
        const InputComboBinding* binding =
            combo_binding_of(input_active_core_profile(input, menu->remap_controller, menu->remap_port), shown_combo);
        if (binding != nullptr)
        {
            lit = input_map_control_for_source(binding->sources[shown_row]);
            minus = binding->sources[shown_row].type == InputSourceType::axis_minus;
        }
    }
    else if (lit == InputMapControl::count && shown_index < INPUT_ACTION_COUNT && shown_row < INPUT_SOURCES_PER_ACTION)
    {
        InputSource source = effective.sources[shown_index][shown_row];
        lit = input_map_control_for_source(source);
        minus = source.type == InputSourceType::axis_minus;
    }
    menu->pad_action = InputAction::count;
    menu->pad_combo.clear();
    float label_width = 110.0f * block;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::BeginChild("remap_rows", ImVec2(0.0f, 0.0f));
    const InputLayout* pad_layout = input_window_layout(input, context->layouts, pad);
    const InputProfile* pad_profile = input_profile(input, input_controller_profile_id(input, menu->remap_controller));
    const char* set = piccolo_text(strings, Str::input_set);
    const char* clear = piccolo_text(strings, Str::input_clear);
    const char* add = "+";
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float padding = ImGui::GetStyle().FramePadding.x * 2.0f;
    float row_buttons = ImGui::CalcTextSize(set).x + ImGui::CalcTextSize(clear).x + padding * 2.0f + spacing;
    float first_buttons = row_buttons + ImGui::CalcTextSize(add).x + padding + spacing;
    float percent_width = ImGui::CalcTextSize("100%").x + padding + ImGui::GetFrameHeight();
    for (size_t i = 0; i < controls.size(); i++)
    {
        InputAction action = controls[i].action;
        uint32_t index = static_cast<uint32_t>(action);
        const InputSource* sources = effective.sources[index];
        bool waiting = menu->capturing && menu->capture_combo.empty() && menu->capture_action == action;
        // A stick direction: a key or button on it pushes it a set amount (the percent).
        bool analog = index >= static_cast<uint32_t>(InputAction::pad_left_x_plus) &&
                      index <= static_cast<uint32_t>(InputAction::pad_right_y_minus);
        uint32_t count = input_sources_count(sources);
        uint32_t rows = count > 0 ? count : 1;
        if (waiting && menu->capture_row >= rows)
        {
            rows = menu->capture_row + 1;   // + : the new row, waiting
        }
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
                }
                ImGui::TextUnformatted(controls[i].label.c_str());
                if (waiting)
                {
                    retro_ui_pop_section_text();
                }
            }
            else
            {
                ImGui::TextUnformatted("");   // more sources of the same control: no label
            }
            float buttons_x = ImGui::GetWindowContentRegionMax().x - (r == 0 ? first_buttons : row_buttons);
            InputSource source = r < count ? sources[r] : InputSource{};
            bool digital = source.type == InputSourceType::key || source.type == InputSourceType::button;
            bool show_percent = analog && digital && !(waiting && menu->capture_row == r);
            float text_end = buttons_x - spacing - (show_percent ? percent_width + spacing : 0.0f);
            ImGui::SameLine(label_width);
            if (waiting && menu->capture_row == r)
            {
                uint64_t elapsed = context->now_ms - menu->capture_started_ms;
                uint64_t total = context->input->settings.press_ms + (menu->bind_all ? REMAP_COOL_OFF_MS : 0);
                uint64_t left = elapsed < total ? (total - elapsed + 999) / 1000 : 0;
                input_window_draw_waiting(pad != nullptr && pad->kind == InputDeviceKind::keyboard, left,
                                          input_capture_hold_progress(input, context->now_ms), strings);
                ImGui::SetScrollHereY(0.5f);   // BIND ALL: keep it in view
            }
            else
            {
                InputSource one[INPUT_SOURCES_PER_ACTION] = {};
                one[0] = source;
                input_window_draw_sources(context->sdl, pad, pad_layout, pad_profile, one, text_end - ImGui::GetCursorPosX(), strings);
            }
            if (show_percent)
            {
                // How far it pushes the stick (D-pad right at 20 %: walking, not running).
                char current[16] = {};
                std::snprintf(current, sizeof(current), "%u%%", static_cast<unsigned>(source.percent));
                ImGui::SameLine(text_end + spacing);
                ImGui::SetNextItemWidth(percent_width);
                ImGui::BeginDisabled(menu->capturing);
                if (retro_ui_begin_combo("##percent", current))
                {
                    for (uint32_t p = 10; p <= 100; p += 10)
                    {
                        char item[16] = {};
                        std::snprintf(item, sizeof(item), "%u%%", static_cast<unsigned>(p));
                        if (ImGui::Selectable(item, source.percent == p))
                        {
                            InputBindingOverride* change = remap_override(menu, context, action);
                            change->sources[r].percent = static_cast<uint8_t>(p);
                        }
                    }
                    ImGui::EndCombo();
                }
                retro_ui_item_tooltip(piccolo_text(strings, Str::remap_percent_help));
                ImGui::EndDisabled();
            }
            ImGui::SameLine(buttons_x > ImGui::GetCursorPosX() ? buttons_x : 0.0f);
            ImGui::BeginDisabled(menu->capturing);
            if (r == 0)
            {
                ImGui::BeginDisabled(count == 0 || count >= INPUT_SOURCES_PER_ACTION);
                if (retro_ui_button(add))
                {
                    menu->bind_all = false;
                    remap_start_capture(menu, context, action, count);   // a new row
                }
                retro_ui_item_tooltip(piccolo_text(strings, Str::input_add_help));
                ImGui::EndDisabled();
                ImGui::SameLine();
            }
            if (retro_ui_button(set))
            {
                menu->bind_all = false;
                remap_start_capture(menu, context, action, r < count ? r : count);
            }
            // The pad shows the row under the mouse, or the one in focus while
            // navigating with a gamepad or keys (a click also focuses, but that
            // focus shouldn't keep the row lit once its SET is over).
            if (ImGui::IsItemHovered() || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible))
            {
                menu->pad_action = action;   // the pad shows it on the next frame
                menu->pad_row = r;
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(r >= count);
            if (retro_ui_button(clear))
            {
                input_sources_remove(remap_override(menu, context, action)->sources, r);
            }
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::PopID();
    }
    draw_combo_rows(menu, context, false, controls, pad, pad_layout, pad_profile, label_width);
    draw_combo_rows(menu, context, true, controls, pad, pad_layout, pad_profile, label_width);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    draw_combo_modal(menu, context, controls, 60.0f * block);
    draw_macro_modal(menu, context, controls, 60.0f * block);
    if (show_pad)
    {
        pad_picture_draw_floating(input_window_layout(input, context->layouts, pad), lit, minus, 1.25f * block);
    }
}

// The buttons at the right of a row, `width` wide: where they start.
static void right_align(float width)
{
    float x = ImGui::GetWindowContentRegionMax().x - width;
    ImGui::SameLine(x > ImGui::GetCursorPosX() ? x : 0.0f);
}

static float button_width(const char* text)
{
    return ImGui::CalcTextSize(text).x + ImGui::GetStyle().FramePadding.x * 2.0f;
}

// CONTROLS: the ports and who is on each (N:N: a port can have several
// controllers, a controller can be on several ports). Per member REMAP and
// LEAVE (that port only), per port ADD (any connected controller not on it)
// and MOVE UP (the whole port). START on a controller joins it.
static void draw_controls_tab(QuickMenu* menu, const QuickMenuContext* context, float block)
{
    if (menu->remap)
    {
        draw_remap(menu, context, block);
        return;
    }
    InputSystem* input = context->input;
    const Localization* strings = context->strings;
    float label_width = 60.0f * block;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    bool can_remap = context->host != nullptr && context->host->game_loaded;
    const char* leave = piccolo_text(strings, Str::input_leave);
    const char* up = piccolo_text(strings, Str::input_move_up);
    const char* remap = piccolo_text(strings, Str::quick_menu_remap);
    bool anybody = false;
    // PORTS: who plays where (the physical controllers).
    retro_ui_separator_text(piccolo_text(strings, Str::quick_menu_ports));
    for (uint32_t p = 0; p < INPUT_PORT_COUNT; p++)
    {
        ImGui::PushID(static_cast<int>(p));
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s %u", piccolo_text(strings, Str::input_port), static_cast<unsigned>(p + 1));
        // (A copy: LEAVE and MOVE UP change the list while it is drawn.)
        std::vector<uint32_t> members = input->ports[p];
        anybody = anybody || !members.empty();
        if (members.empty())
        {
            ImGui::SameLine(label_width);
            ImGui::TextDisabled("%s", piccolo_text(strings, Str::input_port_empty));
        }
        if (p > 0 && (!members.empty() || !input->ports[p - 1].empty()))
        {
            right_align(button_width(up));
            if (retro_ui_button(up))
            {
                input_swap_ports(input, p, p - 1);
            }
        }
        for (size_t m = 0; m < members.size(); m++)
        {
            uint32_t controller = members[m];
            if (controller >= input->controllers.size())
            {
                continue;
            }
            ImGui::PushID(static_cast<int>(controller));
            ImGui::AlignTextToFramePadding();
            ImGui::Indent(label_width);
            ImGui::TextUnformatted(input->controllers[controller].name.c_str());
            ImGui::Unindent(label_width);
            right_align(button_width(leave) + (can_remap ? button_width(remap) + spacing : 0.0f));
            if (can_remap)
            {
                if (retro_ui_button(remap))
                {
                    menu->remap = true;
                    menu->remap_port = p;
                    menu->remap_controller = controller;
                }
                ImGui::SameLine();
            }
            if (retro_ui_button(leave))
            {
                input_port_remove(input, p, controller);
            }
            ImGui::PopID();
        }
        // ADD: a connected controller that isn't on this port (it may be on others).
        std::vector<uint32_t> others;
        for (uint32_t c = 0; c < input->controllers.size(); c++)
        {
            const InputDevice* device = input_devices_find_const(&input->devices, input->controllers[c].device_id);
            if (device != nullptr && device->connected && !input_devices_ignored(device) && !input_port_has(input, p, c))
            {
                others.push_back(c);
            }
        }
        if (!others.empty())
        {
            ImGui::Indent(label_width);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (retro_ui_begin_combo("##add", piccolo_text(strings, Str::input_port_add)))
            {
                for (size_t o = 0; o < others.size(); o++)
                {
                    ImGui::PushID(static_cast<int>(others[o]));
                    if (ImGui::Selectable(input->controllers[others[o]].name.c_str(), false))
                    {
                        input_port_add(input, p, others[o]);
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            ImGui::Unindent(label_width);
        }
        ImGui::PopID();
    }
    // CORE DEVICES: what the core has on each port (only ports where it offers a choice).
    const LibretroHost* host = context->host;
    bool any_choice = false;
    for (uint32_t p = 0; host != nullptr && host->game_loaded && p < host->input_info.port_count && p < INPUT_PORT_COUNT; p++)
    {
        any_choice = any_choice || host->input_info.ports[p].count >= 2;
    }
    if (any_choice)
    {
        ImGui::Spacing();
        retro_ui_separator_text(piccolo_text(strings, Str::quick_menu_core_devices));
        for (uint32_t p = 0; p < INPUT_PORT_COUNT; p++)
        {
            draw_device_combo(context, p, label_width + 40.0f * block);
        }
    }

    if (!anybody)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", piccolo_text(strings, Str::quick_menu_no_ports));
    }
}

// A SET / BIND ALL nobody looks at any more (the menu closed by any of its
// ways, another tab, out of the remap view) is dropped: a capture left on
// would keep every action and the game from the controller.
static void drop_unseen_capture(QuickMenu* menu, const QuickMenuContext* context)
{
    if (menu->capturing && !menu->capture_seen)
    {
        menu->capturing = false;
        menu->bind_all = false;
        input_capture_cancel(context->input);
    }
    menu->capture_seen = false;
    if (menu->macro_record != 0 && !menu->macro_record_seen)
    {
        macro_record_cancel(menu, context);   // (a recording keeps the router suspended)
    }
    menu->macro_record_seen = false;
}

QuickMenuAction quick_menu_draw(QuickMenu* menu, const QuickMenuContext* context, ImFont* pixel_font, int32_t scale)
{
    if (!menu->open)
    {
        drop_unseen_capture(menu, context);
        return QuickMenuAction::none;
    }
    const Localization* strings = context->strings;
    if (menu->refresh)
    {
        menu->refresh = false;
        for (uint32_t slot = LIBRETRO_STATE_SLOT_FIRST; slot <= LIBRETRO_STATE_SLOT_LAST; slot++)
        {
            core_session_slot_info(context->session, slot, &menu->slots[slot]);
        }
        menu->shown_slot = 0;   // the selected thumbnail may have changed
    }

    QuickMenuAction action = QuickMenuAction::none;
    float block = static_cast<float>(scale);
    const ImGuiIO& io = ImGui::GetIO();
    retro_ui_push_theme(pixel_font, scale);
    // About 60 % of the window (never smaller than 360 x 240 retro pixels,
    // never bigger than the window), the same every frame: a window that
    // resizes to its contents loses its scroll.
    ImVec2 size(io.DisplaySize.x * 0.6f, io.DisplaySize.y * 0.6f);
    size.x = size.x > 360.0f * block ? size.x : 360.0f * block;
    size.y = size.y > 240.0f * block ? size.y : 240.0f * block;
    size = ImVec2(static_cast<float>(static_cast<int>(size.x)), static_cast<float>(static_cast<int>(size.y)));
    size.x = size.x < io.DisplaySize.x - 8.0f * block ? size.x : io.DisplaySize.x - 8.0f * block;
    size.y = size.y < io.DisplaySize.y - 24.0f * block ? size.y : io.DisplaySize.y - 24.0f * block;
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    char title[200] = {};
    std::snprintf(title, sizeof(title), "%s###quick_menu", piccolo_text(strings, Str::quick_menu_title));
    if (ImGui::Begin(title, nullptr, flags))
    {
        retro_ui_begin_window(scale);
        if (retro_ui_close_button())
        {
            menu->open = false;
        }
        bool appearing = ImGui::IsWindowAppearing();
        if (appearing)
        {
            ImGui::SetWindowFocus();
        }
        char label[200] = {};
        if (ImGui::BeginTabBar("quick_tabs"))
        {
            int tab_count = 3;
            int target = appearing ? menu->open_on_tab : retro_ui_tab_switch(menu->tab, tab_count);
            // SAVESTATES first: it's what the menu is for. It opens again on the tab it was closed on.
            std::snprintf(label, sizeof(label), "%s###states", piccolo_text(strings, Str::quick_menu_tab_states));
            if (ImGui::BeginTabItem(label, nullptr, target == 0 ? ImGuiTabItemFlags_SetSelected : 0))
            {
                menu->tab = 0;
                action = draw_states_tab(menu, context, block, appearing);
                ImGui::EndTabItem();
            }
            std::snprintf(label, sizeof(label), "%s###core_options", piccolo_text(strings, Str::tab_core_options));
            if (context->core_options_state != nullptr &&
                ImGui::BeginTabItem(label, nullptr, target == 1 ? ImGuiTabItemFlags_SetSelected : 0))
            {
                menu->tab = 1;
                CoreOptionsUiContext core = context->core_options;
                core.strings = strings;
                core.label_width = 80.0f * block;   // SAVE TO / SECTION; the options size their own column
                core_options_ui_draw_running(context->core_options_state, &core);
                ImGui::EndTabItem();
            }
            std::snprintf(label, sizeof(label), "%s###controls", piccolo_text(strings, Str::quick_menu_tab_controls));
            if (context->input != nullptr && ImGui::BeginTabItem(label, nullptr, target == 2 ? ImGuiTabItemFlags_SetSelected : 0))
            {
                menu->tab = 2;
                draw_controls_tab(menu, context, block);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        retro_ui_end_window();
    }
    ImGui::End();
    drop_unseen_capture(menu, context);
    retro_ui_pop_theme();
    return action;
}
