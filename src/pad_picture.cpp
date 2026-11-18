#include "pad_picture.h"

#include "imgui.h"

struct PadPart
{
    InputMapControl control;
    float           x0, y0, x1, y1;   // in a 100 x 46 box
};

// The front, as most pads are.
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

// A six-button face (Genesis, Saturn): A B C below, X Y Z above, on the
// positions the RetroPad gives them; the triggers (Saturn's L / R) sit where
// the shoulders are.
static const PadPart SIX_FACE[] = {
    {InputMapControl::west, 66, 23, 72, 29},
    {InputMapControl::south, 74, 23, 80, 29},
    {InputMapControl::east, 82, 23, 88, 29},
    {InputMapControl::left_shoulder, 66, 14, 72, 20},
    {InputMapControl::north, 74, 14, 80, 20},
    {InputMapControl::right_shoulder, 82, 14, 88, 20},
    {InputMapControl::left_trigger, 12, 5, 30, 9},
    {InputMapControl::right_trigger, 70, 5, 88, 9},
};
constexpr size_t SIX_FACE_COUNT = sizeof(SIX_FACE) / sizeof(SIX_FACE[0]);

// The back, as if the pad were see-through (left stays left): shoulders and
// triggers at the top as on the front, two paddles under each hand, one
// above the other (SDL's paddle1 / paddle3 on the right, paddle2 / paddle4
// on the left).
static const PadPart BACK[] = {
    {InputMapControl::left_trigger, 14, 0, 28, 4},
    {InputMapControl::right_trigger, 72, 0, 86, 4},
    {InputMapControl::left_shoulder, 12, 5, 30, 9},
    {InputMapControl::right_shoulder, 70, 5, 88, 9},
    {InputMapControl::paddle2, 22, 16, 30, 26},
    {InputMapControl::paddle4, 22, 29, 30, 39},
    {InputMapControl::paddle1, 70, 16, 78, 26},
    {InputMapControl::paddle3, 70, 29, 78, 39},
};
constexpr size_t BACK_COUNT = sizeof(BACK) / sizeof(BACK[0]);

// The extra shoulders, next to L / R towards the middle.
static const PadPart EXTRA_SHOULDERS[] = {
    {InputMapControl::l4, 32, 5, 42, 9},
    {InputMapControl::r4, 58, 5, 68, 9},
};
constexpr size_t EXTRA_SHOULDER_COUNT = sizeof(EXTRA_SHOULDERS) / sizeof(EXTRA_SHOULDERS[0]);

static bool replaced_by_six_face(InputMapControl control)
{
    for (size_t p = 0; p < SIX_FACE_COUNT; p++)
    {
        if (SIX_FACE[p].control == control)
        {
            return true;
        }
    }
    return false;
}

static void draw_part(ImDrawList* draw_list, ImVec2 origin, float unit, const PadPart* part, ImU32 color)
{
    draw_list->AddRectFilled(ImVec2(origin.x + part->x0 * unit, origin.y + part->y0 * unit),
                             ImVec2(origin.x + part->x1 * unit, origin.y + part->y1 * unit), color);
}

// The drawing at `origin` (its top left, 100 x 46 units), the body and its
// parts at `alpha` (the lit part always solid).
static void draw_pad(ImDrawList* draw_list, ImVec2 origin, const InputLayout* layout, PadView view, InputMapControl lit,
                     bool minus, float unit, float alpha)
{
    ImU32 body = ImGui::GetColorU32(ImGuiCol_Button, alpha);
    ImU32 part = ImGui::GetColorU32(ImGuiCol_FrameBg, alpha);
    ImU32 light = IM_COL32(255, 220, 80, 255);   // the same yellow in every theme (and in Test libretro)

    if (view == PadView::back)
    {
        draw_list->AddRectFilled(ImVec2(origin.x + 8.0f * unit, origin.y + 10.0f * unit),
                                 ImVec2(origin.x + 92.0f * unit, origin.y + 44.0f * unit), body);
        for (size_t p = 0; p < BACK_COUNT; p++)
        {
            draw_part(draw_list, origin, unit, &BACK[p], BACK[p].control == lit ? light : part);
        }
        return;
    }

    // The body, and the d-pad's middle.
    draw_list->AddRectFilled(ImVec2(origin.x + 8.0f * unit, origin.y + 10.0f * unit),
                             ImVec2(origin.x + 92.0f * unit, origin.y + 44.0f * unit), body);
    draw_list->AddRectFilled(ImVec2(origin.x + 19.0f * unit, origin.y + 19.0f * unit),
                             ImVec2(origin.x + 25.0f * unit, origin.y + 25.0f * unit), part);
    bool six = layout != nullptr && layout->six_face;
    for (size_t p = 0; p < PART_COUNT; p++)
    {
        // Only what this kind of pad has (a stick: its click or its axes).
        InputMapControl control = PARTS[p].control;
        bool present = layout == nullptr || control == lit || input_layout_has(layout, control) ||
                       (control == InputMapControl::left_stick && input_layout_has(layout, InputMapControl::left_x)) ||
                       (control == InputMapControl::right_stick && input_layout_has(layout, InputMapControl::right_x));
        if (!present || (six && replaced_by_six_face(control)))
        {
            continue;
        }
        // A stick lights for its click and for pushing it.
        bool on = control == lit ||
                  (control == InputMapControl::left_stick && (lit == InputMapControl::left_x || lit == InputMapControl::left_y)) ||
                  (control == InputMapControl::right_stick && (lit == InputMapControl::right_x || lit == InputMapControl::right_y));
        draw_part(draw_list, origin, unit, &PARTS[p], on ? light : part);
    }
    for (size_t p = 0; six && p < SIX_FACE_COUNT; p++)
    {
        if (input_layout_has(layout, SIX_FACE[p].control))
        {
            draw_part(draw_list, origin, unit, &SIX_FACE[p], SIX_FACE[p].control == lit ? light : part);
        }
    }
    if (view == PadView::extra_shoulders || lit == InputMapControl::l4 || lit == InputMapControl::r4)
    {
        for (size_t p = 0; p < EXTRA_SHOULDER_COUNT; p++)
        {
            draw_part(draw_list, origin, unit, &EXTRA_SHOULDERS[p], EXTRA_SHOULDERS[p].control == lit ? light : part);
        }
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
}

void pad_picture_draw_view(const InputLayout* layout, PadView view, InputMapControl lit, bool minus, float unit)
{
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    origin.x += static_cast<float>(static_cast<int>((width - 100.0f * unit) * 0.5f));
    draw_pad(ImGui::GetWindowDrawList(), origin, layout, view, lit, minus, unit, 1.0f);
    ImGui::Dummy(ImVec2(width, 46.0f * unit));
}

// A paddle lit shows the pad from behind; L4 / R4, the extra shoulders.
static PadView view_for(InputMapControl lit)
{
    bool paddle = lit == InputMapControl::paddle1 || lit == InputMapControl::paddle2 || lit == InputMapControl::paddle3 ||
                  lit == InputMapControl::paddle4;
    return paddle ? PadView::back : PadView::front;
}

void pad_picture_draw_floating(const InputLayout* layout, InputMapControl lit, bool minus, float unit)
{
    const ImGuiIO& io = ImGui::GetIO();
    float margin = 4.0f * unit;
    ImVec2 origin(static_cast<float>(static_cast<int>(io.DisplaySize.x - 100.0f * unit - margin)),
                  static_cast<float>(static_cast<int>(io.DisplaySize.y - 46.0f * unit - margin)));
    draw_pad(ImGui::GetForegroundDrawList(), origin, layout, view_for(lit), lit, minus, unit, 0.8f);
}

void pad_picture_draw(const InputLayout* layout, InputMapControl lit, bool minus, float unit)
{
    pad_picture_draw_view(layout, view_for(lit), lit, minus, unit);
}
