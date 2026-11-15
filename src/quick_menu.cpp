#include "quick_menu.h"

#include <cstdio>
#include <cstring>

#include <SDL3/SDL.h>

#include "imgui.h"

#include "app_shell.h"
#include "debug_ui.h"
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

void quick_menu_open(QuickMenu* menu, uint32_t slot)
{
    menu->open_on_tab = 0;
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

    // The thumbnail in a 4:3 box (letterboxed to its own aspect), the slot's details beside it.
    const StateSlotInfo* info = &menu->slots[menu->slot];
    ImVec2 box(160.0f * block, 120.0f * block);
    ImVec2 corner = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(corner, ImVec2(corner.x + box.x, corner.y + box.y), IM_COL32(0, 0, 0, 255));
    if (menu->has_picture && menu->shown_slot == menu->slot && menu->texture_id != 0 && menu->height > 0)
    {
        float aspect = static_cast<float>(menu->width) / static_cast<float>(menu->height);
        ImVec2 picture(box.x, box.x / aspect);
        if (picture.y > box.y)
        {
            picture = ImVec2(box.y * aspect, box.y);
        }
        ImVec2 top_left(corner.x + (box.x - picture.x) * 0.5f, corner.y + (box.y - picture.y) * 0.5f);
        float u = 0.0f;
        float v = 0.0f;
        streaming_texture_uv_max(&menu->texture, &u, &v);
        draw_list->AddImage(ImTextureRef(static_cast<ImTextureID>(menu->texture_id)), top_left,
                            ImVec2(top_left.x + picture.x, top_left.y + picture.y), ImVec2(0.0f, 0.0f), ImVec2(u, v));
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

// CONTROLS: the ports, who is on each (the keyboard too, when it shares
// port 1), MOVE UP and LEAVE. START on a controller joins it.
static void draw_controls_tab(const QuickMenuContext* context, float block)
{
    InputSystem* input = context->input;
    const Localization* strings = context->strings;
    float label_width = 60.0f * block;
    bool anybody = false;
    for (uint32_t p = 0; p < INPUT_PORT_COUNT; p++)
    {
        ImGui::PushID(static_cast<int>(p));
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s %u", piccolo_text(strings, Str::input_port), static_cast<unsigned>(p + 1));
        ImGui::SameLine(label_width);
        int32_t controller = input->ports[p];
        bool keyboard_here = p == 0 && input->settings.keyboard_shares_port1;
        if (controller < 0)
        {
            if (keyboard_here)
            {
                ImGui::TextUnformatted(piccolo_text(strings, Str::input_keyboard_shared));
            }
            else
            {
                ImGui::TextDisabled("%s", piccolo_text(strings, Str::input_port_empty));
            }
            ImGui::PopID();
            continue;
        }
        anybody = true;
        if (keyboard_here)
        {
            ImGui::Text("%s + %s", input->controllers[static_cast<size_t>(controller)].name.c_str(),
                        piccolo_text(strings, Str::input_keyboard_shared));
        }
        else
        {
            ImGui::TextUnformatted(input->controllers[static_cast<size_t>(controller)].name.c_str());
        }
        const char* leave = piccolo_text(strings, Str::input_leave);
        const char* up = piccolo_text(strings, Str::input_move_up);
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float pad = ImGui::GetStyle().FramePadding.x * 2.0f;
        float width = ImGui::CalcTextSize(leave).x + pad + (p > 0 ? ImGui::CalcTextSize(up).x + pad + spacing : 0.0f);
        float x = ImGui::GetWindowContentRegionMax().x - width;
        ImGui::SameLine(x > ImGui::GetCursorPosX() ? x : 0.0f);
        if (p > 0)
        {
            if (retro_ui_button(up))
            {
                input_swap_ports(input, p, p - 1);
            }
            ImGui::SameLine();
        }
        if (retro_ui_button(leave))
        {
            input_leave(input, static_cast<uint32_t>(controller));
        }
        ImGui::PopID();
    }
    // DEVICE per port: what the core emulates there (only ports it describes).
    LibretroHost* host = context->host;
    if (host != nullptr && host->game_loaded)
    {
        for (uint32_t p = 0; p < host->input_info.port_count && p < INPUT_PORT_COUNT; p++)
        {
            const LibretroPortControllers* types = &host->input_info.ports[p];
            if (types->count < 2)
            {
                continue;   // nothing to choose
            }
            ImGui::PushID(static_cast<int>(100 + p));
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s %u", piccolo_text(strings, Str::quick_menu_device), static_cast<unsigned>(p + 1));
            ImGui::SameLine(label_width + 20.0f * block);
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
    }

    if (!anybody && !input->settings.keyboard_shares_port1)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", piccolo_text(strings, Str::quick_menu_no_ports));
    }
}

QuickMenuAction quick_menu_draw(QuickMenu* menu, const QuickMenuContext* context, ImFont* pixel_font, int32_t scale)
{
    if (!menu->open)
    {
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
    // A fixed size like SETTINGS (360 x 240 retro pixels, never bigger than
    // the window): a window that resizes to its contents loses its scroll.
    ImVec2 size(360.0f * block, 240.0f * block);
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
            // SAVESTATES first: it's what the menu is for. Opening it again starts there.
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
                draw_controls_tab(context, block);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        retro_ui_end_window();
    }
    ImGui::End();
    retro_ui_pop_theme();
    return action;
}
