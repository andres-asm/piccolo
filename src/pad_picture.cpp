#include "pad_picture.h"

#include "imgui.h"
#include "retro_ui.h"

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

void pad_picture_draw(const InputLayout* layout, InputMapControl lit, bool minus, float unit)
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
    if (input_map_control_is_stick(lit))
    {
        bool left = lit == InputMapControl::left_x || lit == InputMapControl::left_y;
        bool x = lit == InputMapControl::left_x || lit == InputMapControl::right_x;
        float cx = left ? 36.0f : 64.0f;
        float mx = x ? (minus ? cx - 7.0f : cx + 5.0f) : cx - 1.0f;
        float my = x ? 33.0f : (minus ? 27.0f : 39.0f);
        draw_list->AddRectFilled(ImVec2(origin.x + mx * unit, origin.y + my * unit),
                                 ImVec2(origin.x + (mx + 2.0f) * unit, origin.y + (my + 2.0f) * unit), light);
    }
    ImGui::Dummy(ImVec2(width, 46.0f * unit));
}
