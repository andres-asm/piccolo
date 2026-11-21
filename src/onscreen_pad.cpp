#include "onscreen_pad.h"

#include <SDL3/SDL.h>

#include "imgui.h"
#include "imgui_internal.h"   // BringWindowToDisplayFront (EDIT LAYOUT on top)
#include "input_devices.h"
#include "pad_layout.h"
#include "retro_ui.h"
#include "touch_ui.h"

// Upright, the pad goes under the picture only if it's still this big
// (retro pixels per dp; piccoletto's REMOTE PLAY uses the same).
constexpr float MIN_BELOW_SCALE = 0.3f;
constexpr float BELOW_GAP = 4.0f;   // retro pixels between the picture and the pad

void onscreen_pad_init(OnscreenPad* pad, const char* settings_dir)
{
    pad_layouts_init(&pad->layouts);
    if (settings_dir[0] != '\0')
    {
        pad->path = std::string(settings_dir) + "/touch_layouts.cfg";
        settings_file_load(&pad->file, pad->path.c_str());   // (none yet: the defaults)
        for (int32_t i = 0; i < PAD_LAYOUT_COUNT; i++)
        {
            pad_layout_load(&pad->layouts.layouts[i], &pad->file);
        }
    }
    haptics_open(&pad->haptics);
    // A touch screen (AUTO): asked once here (SDL allocates the list); a
    // finger later says so too.
    int count = 0;
    SDL_TouchID* touches = SDL_GetTouchDevices(&count);
    SDL_free(touches);
    pad->touch_screen = count > 0;
}

void onscreen_pad_shutdown(OnscreenPad* pad)
{
    haptics_close(&pad->haptics);
}

static void save_layouts(OnscreenPad* pad)
{
    if (pad->path.empty())
    {
        return;
    }
    for (int32_t i = 0; i < PAD_LAYOUT_COUNT; i++)
    {
        pad_layout_save(&pad->layouts.layouts[i], &pad->file);
    }
    settings_file_save(&pad->file, pad->path.c_str(), "piccolo's on-screen gamepad layouts (EDIT LAYOUT)");
}

void onscreen_pad_event(OnscreenPad* pad, const SDL_Event* event, float display_width, float display_height)
{
    if (event->type == SDL_EVENT_FINGER_DOWN)
    {
        pad->touch_screen = true;
    }
    touch_fingers_event(&pad->fingers, event, display_width, display_height);
}

bool onscreen_pad_enabled(const OnscreenPad* pad, OnscreenPadShow show)
{
    if (show == OnscreenPadShow::on)
    {
        return true;
    }
    if (show == OnscreenPadShow::off)
    {
        return false;
    }
    return pad->touch_screen;   // AUTO
}

void onscreen_pad_edit_end(OnscreenPad* pad)
{
    if (pad->editor.active)
    {
        pad->editor.active = false;
        save_layouts(pad);
    }
}

// A real gamepad played (a button held): AUTO hides the pad.
static bool real_gamepad_played(const OnscreenPad* pad, const InputSystem* input)
{
    for (size_t i = 0; i < input->devices.devices.size(); i++)
    {
        const InputDevice* device = &input->devices.devices[i];
        if (device->kind != InputDeviceKind::gamepad || !device->connected || device->game_only || device->retropad ||
            (pad->plugged && device->id == pad->device))
        {
            continue;
        }
        for (uint32_t b = 0; b < INPUT_BUTTON_COUNT; b++)
        {
            if (device->held[b] != 0)
            {
                return true;
            }
        }
    }
    return false;
}

void onscreen_pad_update(OnscreenPad* pad, InputSystem* input, bool enabled, OnscreenPadShow show, bool playing)
{
    if (enabled && !pad->plugged)
    {
        pad->device = input_devices_add(&input->devices, InputDeviceKind::gamepad, "touch", "TOUCH PAD");
        InputDevice* device = input_devices_find(&input->devices, pad->device);
        if (device != nullptr)
        {
            device->retropad = true;               // hands a RetroPad (the layouts' parts)
            device->ignore = InputIgnore::no;      // (a finger held on a stick isn't a stuck receiver)
        }
        input_add_controllers_for_device(input, pad->device);
        pad->plugged = true;
        SDL_Log("Touch pad: plugged in");
    }
    else if (!enabled && pad->plugged)
    {
        input_devices_disconnect(&input->devices, pad->device);   // (plugged again later: the same device comes back)
        pad->plugged = false;
        SDL_Log("Touch pad: unplugged");
    }
    // AUTO: a real gamepad hides it, a finger on the screen brings it back.
    if (show == OnscreenPadShow::automatic)
    {
        if (real_gamepad_played(pad, input))
        {
            pad->hidden = true;
        }
        for (int32_t f = 0; f < pad->fingers.count; f++)
        {
            pad->hidden = pad->hidden && !pad->fingers.fingers[f].began;
        }
    }
    else
    {
        pad->hidden = false;
    }
    if (pad->plugged)
    {
        RetroPad none;
        const RetroPad* state = playing && !pad->hidden && !pad->editor.active ? &pad->pad : &none;
        input_devices_set_retropad(&input->devices, pad->device, state->buttons, state->analog);
    }
}

void onscreen_pad_place(OnscreenPad* pad, bool playing, float aspect, float width, float height)
{
    pad->shown = pad->editor.active || (pad->plugged && playing && !pad->hidden);
    pad->below = false;
    pad->picture_height = height;
    if (width >= height)
    {
        pad->layout = PadLayoutId::remote_overlay_landscape;
        return;
    }
    // Upright: the picture across the top; does the pad still fit under it?
    float picture = aspect > 0.0f ? width / aspect : width;
    picture = picture > height ? height : picture;
    int32_t scale = touch_ui_scale_for(width, height);
    float block = static_cast<float>(scale);
    const PadLayout* below = pad_layouts_get(&pad->layouts, PadLayoutId::remote_below);
    float room = (height - picture) / block - BELOW_GAP;
    PadFrame frame = pad_frame_fit_into(below->width, below->height, 0.0f, 0.0f, width / block, room > 0.0f ? room : 0.0f);
    if (frame.dp_scale >= MIN_BELOW_SCALE)
    {
        pad->layout = PadLayoutId::remote_below;
        pad->below = pad->shown;
        pad->picture_height = pad->shown ? picture : height;
        return;
    }
    pad->layout = PadLayoutId::remote_overlay_portrait;   // a tall picture: over its lower part
}

void onscreen_pad_draw(OnscreenPad* pad, ImFont* pixel_font, const char* menu_label, float opacity, bool vibrate)
{
    pad->menu_pressed = false;
    pad->edit_done = false;
    pad->pad = RetroPad{};
    if (!pad->shown)
    {
        return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    TouchUi ui;
    ui.font = pixel_font;
    ui.scale = touch_ui_scale_for(io.DisplaySize.x, io.DisplaySize.y);
    ui.palette = *retro_ui_palette();
    ui.fingers = &pad->fingers;
    ui.pad = &pad->pad;
    for (int32_t i = 0; i < PAD_LAYOUT_COUNT; i++)
    {
        PadLayout* each = &pad->layouts.layouts[i];
        each->default_alpha = opacity;
        if (!each->alpha_set)
        {
            each->alpha = opacity;   // (a layout's own, from EDIT LAYOUT, wins)
        }
    }
    PadLayout* layout = pad_layouts_get(&pad->layouts, pad->layout);
    layout->action_label = menu_label;   // retroui's action button: the QUICK MENU
    float width = touch_ui_width(&ui);
    float height = touch_ui_height(&ui);
    PadFrame frame;
    // The overlay window takes the taps meanwhile (nothing under it reacts).
    touch_ui_begin_overlay(&ui, "##onscreen_pad");
    if (pad->editor.active)
    {
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());   // EDIT LAYOUT: above everything
    }
    if (pad->below)
    {
        float top = pad->picture_height / static_cast<float>(ui.scale) + BELOW_GAP;
        touch_ui_rect(&ui, 0.0f, top - BELOW_GAP, width, height - top + BELOW_GAP, ui.palette.button_face);   // the pad's body
        frame = pad_frame_fit_into(layout->width, layout->height, 0.0f, top, width, height - top);
    }
    else if (pad->layout == PadLayoutId::remote_overlay_landscape)
    {
        frame = pad_frame_fit_bottom(&ui, layout->width, layout->height);
    }
    else
    {
        frame = pad_frame_fit(&ui, layout->width, layout->height);
    }
    if (pad->editor.active)
    {
        if (pad_editor_draw(&pad->editor, &ui, &frame, layout))
        {
            pad->editor.active = false;
            pad->edit_done = true;
            save_layouts(pad);
        }
    }
    else
    {
        pad_layout_draw(&ui, &frame, layout);
        pad->menu_pressed = layout->action_pressed;
        if (vibrate && pad->fingers.presses > 0)
        {
            haptics_buzz(&pad->haptics);
        }
    }
    touch_ui_end_screen();
}

void onscreen_pad_end_frame(OnscreenPad* pad)
{
    touch_fingers_end_frame(&pad->fingers);
}

void onscreen_pad_edit(OnscreenPad* pad)
{
    pad->editor = PadEditor{};
    pad->editor.active = true;
    pad->hidden = false;
}
