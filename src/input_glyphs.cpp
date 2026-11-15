#include "input_glyphs.h"

#include <cstring>

#include <SDL3/SDL.h>

#include "imgui.h"

#include "app_shell.h"
#include "debug_ui.h"
#include "gamepad_check.h"
#include "host_fs.h"

// A stick counts as "used" past about half of its range (not drift).
constexpr int32_t STICK_USED = 16000;

// The pictures for each action, by file name (up to two: the keyboard has
// no "up and down" key, so it shows both). Order = GlyphAction.
struct ActionFiles
{
    const char* gamepad;
    const char* keyboard_first;
    const char* keyboard_second;   // nullptr = one picture
};

static const ActionFiles ACTION_FILES[] = {
    {"south", "return", nullptr},          // confirm
    {"east", "escape", nullptr},           // back
    {"dpad_updown", "up", "down"},         // move_vertical
    {"dpad_leftright", "left", "right"},   // move_horizontal
    {"start", "escape", nullptr},          // menu
    {"west", "space", nullptr},            // alternate
};
static_assert(sizeof(ACTION_FILES) / sizeof(ACTION_FILES[0]) == static_cast<size_t>(GlyphAction::count),
              "one entry per GlyphAction");

// SDL's gamepad type -> the folder with that pad's pictures. Pads SDL can't
// name (generic, Steam...) get the Xbox Series ones: the layout most PC pads copy.
static const char* pad_folder_for(SDL_GamepadType type)
{
    switch (type)
    {
        case SDL_GAMEPAD_TYPE_XBOX360:
            return "xbox360";
        case SDL_GAMEPAD_TYPE_PS3:
            return "ps3";
        case SDL_GAMEPAD_TYPE_PS4:
            return "ps4";
        case SDL_GAMEPAD_TYPE_PS5:
            return "ps5";
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
            return "switch";
        case SDL_GAMEPAD_TYPE_GAMECUBE:
            return "gc";
        default:
            return "xboxseries";
    }
}

static void use_gamepad(InputGlyphs* glyphs, SDL_JoystickID id)
{
    glyphs->device = InputDeviceKind::gamepad;
    glyphs->pad_folder = pad_folder_for(SDL_GetGamepadTypeForID(id));
}

void input_glyphs_handle_event(InputGlyphs* glyphs, const SDL_Event* event)
{
    switch (event->type)
    {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            glyphs->any_input = true;
            glyphs->device = InputDeviceKind::keyboard;
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            glyphs->any_input = true;
            use_gamepad(glyphs, event->gbutton.which);
            break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (event->gaxis.value > STICK_USED || event->gaxis.value < -STICK_USED)
            {
                glyphs->any_input = true;
                use_gamepad(glyphs, event->gaxis.which);
            }
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            // On a TV with only a pad, the hints are right from the start.
            // Not for a fake pad (a keyboard with a stuck "stick": gamepad_check.h);
            // those are never opened by anyone else, so they send no buttons either.
            if (!glyphs->any_input && gamepad_check_id(event->gdevice.which))
            {
                use_gamepad(glyphs, event->gdevice.which);
            }
            break;
        default:
            break;
    }
}

void input_glyphs_set_language(InputGlyphs* glyphs, const char* language)
{
    glyphs->language = language;
}

// assets/glyphs/ next to the program.
static std::string glyphs_folder()
{
    const char* base = SDL_GetBasePath();   // ends with a separator; SDL owns it
    return std::string(base != nullptr ? base : "") + "assets/glyphs/";
}

// The keyboard picture for `key`: the language's own label first, then
// English, then the language-neutral one (see the glyphs README).
static std::string keyboard_file(const InputGlyphs* glyphs, const std::string& folder, const char* key)
{
    std::string candidates[3] = {
        folder + "keyboard/" + glyphs->language + "/" + key + ".png",
        folder + "keyboard/en/" + key + ".png",
        folder + "keyboard/common/" + key + ".png",
    };
    for (int i = 0; i < 3; i++)
    {
        if (host_fs_exists(candidates[i].c_str()))
        {
            return candidates[i];
        }
    }
    return std::string();
}

// Copies one 32x32 PNG into column `slot` of the pixels. False (and logged)
// if it can't be read or has the wrong size.
static bool load_into_slot(InputGlyphs* glyphs, const std::string& path, int32_t slot)
{
    SDL_Surface* png = SDL_LoadPNG(path.c_str());
    SDL_Surface* converted = png != nullptr ? SDL_ConvertSurface(png, SDL_PIXELFORMAT_ARGB8888) : nullptr;
    bool ok = converted != nullptr && converted->w == static_cast<int>(INPUT_GLYPH_SIZE) &&
              converted->h == static_cast<int>(INPUT_GLYPH_SIZE);
    if (ok)
    {
        uint32_t stride = INPUT_GLYPH_SLOTS * INPUT_GLYPH_SIZE;
        for (uint32_t y = 0; y < INPUT_GLYPH_SIZE; y++)
        {
            const uint8_t* row = static_cast<const uint8_t*>(converted->pixels) + static_cast<size_t>(y) * static_cast<size_t>(converted->pitch);
            uint32_t* target = &glyphs->pixels[static_cast<size_t>(y) * stride + static_cast<size_t>(slot) * INPUT_GLYPH_SIZE];
            std::memcpy(target, row, INPUT_GLYPH_SIZE * 4);
        }
    }
    else
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Glyphs: could not read %s (32x32 PNG): %s", path.c_str(), SDL_GetError());
    }
    SDL_DestroySurface(converted);
    SDL_DestroySurface(png);
    return ok;
}

void input_glyphs_update(InputGlyphs* glyphs)
{
    bool keyboard = glyphs->device == InputDeviceKind::keyboard;
    std::string key = keyboard ? "keyboard/" + glyphs->language : glyphs->pad_folder;
    if (glyphs->loaded && key == glyphs->loaded_key)
    {
        return;
    }
    glyphs->loaded = true;
    glyphs->loaded_key = key;
    glyphs->pixels.assign(static_cast<size_t>(INPUT_GLYPH_SLOTS) * INPUT_GLYPH_SIZE * INPUT_GLYPH_SIZE, 0u);

    std::string folder = glyphs_folder();
    int32_t next_slot = 0;
    for (int action = 0; action < static_cast<int>(GlyphAction::count); action++)
    {
        glyphs->first_slot[action] = next_slot;
        glyphs->slot_count[action] = 0;
        const ActionFiles* files = &ACTION_FILES[action];
        const char* names[2] = {keyboard ? files->keyboard_first : files->gamepad,
                                keyboard ? files->keyboard_second : nullptr};
        for (int i = 0; i < 2; i++)
        {
            if (names[i] == nullptr || next_slot >= static_cast<int32_t>(INPUT_GLYPH_SLOTS))
            {
                continue;
            }
            std::string path = keyboard ? keyboard_file(glyphs, folder, names[i])
                                        : folder + glyphs->pad_folder + "/" + names[i] + ".png";
            // A pad without that button simply has no file: no glyph, no warning.
            if (path.empty() || !host_fs_exists(path.c_str()))
            {
                continue;
            }
            if (load_into_slot(glyphs, path, next_slot))
            {
                glyphs->slot_count[action]++;
                next_slot++;
            }
        }
    }
    glyphs->upload_pending = true;
}

void input_glyphs_upload(InputGlyphs* glyphs, AppShell* shell)
{
    if (!glyphs->upload_pending)
    {
        return;
    }
    if (glyphs->texture.image == VK_NULL_HANDLE)
    {
        if (!streaming_texture_create(&glyphs->texture, &shell->vulkan, INPUT_GLYPH_SLOTS * INPUT_GLYPH_SIZE, INPUT_GLYPH_SIZE))
        {
            glyphs->upload_pending = false;
            return;
        }
        glyphs->texture_id = debug_ui_add_texture(glyphs->texture.view);
    }
    if (!app_shell_acquire(shell))
    {
        return;   // try on the next frame
    }
    if (streaming_texture_upload(&glyphs->texture, renderer_command_buffer(&shell->renderer),
                                 renderer_frame_index(&shell->renderer), glyphs->pixels.data(),
                                 INPUT_GLYPH_SLOTS * INPUT_GLYPH_SIZE, INPUT_GLYPH_SIZE))
    {
        glyphs->has_picture = true;
    }
    glyphs->upload_pending = false;
}

void input_glyphs_destroy(InputGlyphs* glyphs, AppShell* shell)
{
    if (glyphs->texture.image != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(shell->vulkan.device);
        if (glyphs->texture_id != 0)
        {
            debug_ui_remove_texture(glyphs->texture_id);
        }
        streaming_texture_destroy(&glyphs->texture, &shell->vulkan);
    }
    glyphs->texture_id = 0;
    glyphs->has_picture = false;
}

float input_glyphs_size(int32_t scale)
{
    // A text line's height (8 retro pixels), never below 16: 24 at 1080p.
    // 32 stood out more than the words next to them, 16 was too small
    // (Andrés, 2026-11-15). At some scales (24, 48) the 32-pixel pictures
    // are scaled by a fraction; nearest filtering keeps them hard-edged.
    int32_t size = 8 * scale;
    return static_cast<float>(size < 16 ? 16 : size);
}

float input_glyphs_width(const InputGlyphs* glyphs, GlyphAction action, float size, float gap)
{
    int32_t count = glyphs->slot_count[static_cast<int>(action)];
    if (count <= 0)
    {
        return 0.0f;
    }
    return static_cast<float>(count) * size + static_cast<float>(count - 1) * gap;
}

float input_glyphs_draw(const InputGlyphs* glyphs, ImDrawList* draw_list, GlyphAction action, float x, float y,
                        float size, float gap, uint32_t color)
{
    int32_t count = glyphs->slot_count[static_cast<int>(action)];
    if (count <= 0 || !glyphs->has_picture || glyphs->texture_id == 0)
    {
        return input_glyphs_width(glyphs, action, size, gap);
    }
    ImU32 tint = IM_COL32((color >> 16) & 0xFFu, (color >> 8) & 0xFFu, color & 0xFFu, 0xFFu);
    float u_step = 1.0f / static_cast<float>(INPUT_GLYPH_SLOTS);
    float left = x;
    for (int32_t i = 0; i < count; i++)
    {
        float u = static_cast<float>(glyphs->first_slot[static_cast<int>(action)] + i) * u_step;
        draw_list->AddImage(ImTextureRef(static_cast<ImTextureID>(glyphs->texture_id)), ImVec2(left, y),
                            ImVec2(left + size, y + size), ImVec2(u, 0.0f), ImVec2(u + u_step, 1.0f), tint);
        left += size + gap;
    }
    return input_glyphs_width(glyphs, action, size, gap);
}
