#include "image_window.h"

#include <cstdio>
#include <cstdlib>

#include "imgui.h"

#include "piccolo_settings.h"
#include "piccolo_text.h"
#include "retro_ui.h"

// Shading over the cut-off parts, and the outline of what stays (0xAABBGGRR,
// ImGui's packed order).
constexpr ImU32 CROP_SHADE   = 0xC0000000u;
constexpr ImU32 CROP_OUTLINE = 0xFF40C0F0u;   // amber

static const char* const CROP_KEYS[] = {PICCOLO_VIDEO_CROP_TOP, PICCOLO_VIDEO_CROP_BOTTOM, PICCOLO_VIDEO_CROP_LEFT,
                                        PICCOLO_VIDEO_CROP_RIGHT};
constexpr int32_t CROP_KEY_COUNT = 4;

// The crop as it resolves at the scope being edited (what this window edits).
static float crop_at_scope(const SettingsUiContext* context, const char* key)
{
    const char* value = settings_get_at(context->settings, context->scope, key, nullptr);
    return value != nullptr ? static_cast<float>(std::strtol(value, nullptr, 10)) : 0.0f;
}

// The frame at `factor`, with the crop shaded and outlined, at the cursor.
static void draw_preview(const SettingsUiContext* context, const ImageWindowSource* source, float factor, float block)
{
    float width = static_cast<float>(source->frame_width) * factor;
    float height = static_cast<float>(source->frame_height) * factor;
    ImVec2 top_left = ImGui::GetCursorScreenPos();
    ImVec2 bottom_right(top_left.x + width, top_left.y + height);
    ImGui::Dummy(ImVec2(width, height));

    // retro_ui_begin_window() switched this window to nearest sampling: sharp pixels.
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddImage(ImTextureRef(static_cast<ImTextureID>(source->texture_id)), top_left, bottom_right,
                        ImVec2(0.0f, 0.0f), ImVec2(source->uv_max_x, source->uv_max_y));

    float top = crop_at_scope(context, PICCOLO_VIDEO_CROP_TOP) * factor;
    float bottom = crop_at_scope(context, PICCOLO_VIDEO_CROP_BOTTOM) * factor;
    float left = crop_at_scope(context, PICCOLO_VIDEO_CROP_LEFT) * factor;
    float right = crop_at_scope(context, PICCOLO_VIDEO_CROP_RIGHT) * factor;
    // Same rule as the viewport: a crop that leaves nothing on an axis is ignored there.
    if (top + bottom >= height)
    {
        top = 0.0f;
        bottom = 0.0f;
    }
    if (left + right >= width)
    {
        left = 0.0f;
        right = 0.0f;
    }
    ImVec2 keep_min(top_left.x + left, top_left.y + top);
    ImVec2 keep_max(bottom_right.x - right, bottom_right.y - bottom);
    draw_list->AddRectFilled(top_left, ImVec2(bottom_right.x, keep_min.y), CROP_SHADE);
    draw_list->AddRectFilled(ImVec2(top_left.x, keep_max.y), bottom_right, CROP_SHADE);
    draw_list->AddRectFilled(ImVec2(top_left.x, keep_min.y), ImVec2(keep_min.x, keep_max.y), CROP_SHADE);
    draw_list->AddRectFilled(ImVec2(keep_max.x, keep_min.y), ImVec2(bottom_right.x, keep_max.y), CROP_SHADE);
    draw_list->AddRect(keep_min, keep_max, CROP_OUTLINE, 0.0f, 0, block);
}

// The image controls, in the column right of the picture. Each group is a
// collapsible section; more join them later.
static void draw_controls(const SettingsUiContext* context)
{
    char header[200] = {};
    std::snprintf(header, sizeof(header), "%s###crop", piccolo_text(context->strings, Str::image_section_crop));
    if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (int32_t i = 0; i < CROP_KEY_COUNT; i++)
        {
            settings_ui_row(context, CROP_KEYS[i], nullptr);
        }
    }
}

void image_window_draw(bool* open, const SettingsUiContext* context, const ImageWindowSource* source,
                       ImFont* pixel_font, int32_t scale)
{
    if (!*open)
    {
        return;
    }
    float block = static_cast<float>(scale);
    const ImGuiIO& io = ImGui::GetIO();
    retro_ui_push_theme(pixel_font, scale);

    // Layout: the picture on the left, the controls' column on the right.
    // The picture gets the biggest whole factor that fits next to the column
    // in piccolo's window.
    const ImGuiStyle& style = ImGui::GetStyle();
    float controls_width = 190.0f * block;
    float controls_height = static_cast<float>(CROP_KEY_COUNT + 1) * ImGui::GetFrameHeightWithSpacing();
    float chrome_x = style.WindowPadding.x * 2.0f + style.ItemSpacing.x;
    float chrome_y = style.WindowPadding.y * 2.0f + ImGui::GetFrameHeight() +   // title bar
                     ImGui::GetTextLineHeightWithSpacing();                      // "EDITING: ..."
    float max_width = io.DisplaySize.x - 8.0f * block;
    float max_height = io.DisplaySize.y - 16.0f * block;
    bool has_picture = source->texture_id != 0 && source->frame_width > 0 && source->frame_height > 0;
    float factor = 1.0f;
    if (has_picture)
    {
        float fit_x = (max_width - chrome_x - controls_width) / static_cast<float>(source->frame_width);
        float fit_y = (max_height - chrome_y) / static_cast<float>(source->frame_height);
        float fit = fit_x < fit_y ? fit_x : fit_y;
        factor = fit >= 1.0f ? static_cast<float>(static_cast<int32_t>(fit)) : 1.0f;
    }
    ImVec2 picture(has_picture ? static_cast<float>(source->frame_width) * factor : 160.0f * block,
                   has_picture ? static_cast<float>(source->frame_height) * factor : controls_height);
    float body_height = picture.y > controls_height ? picture.y : controls_height;
    ImVec2 size(picture.x + controls_width + chrome_x, body_height + chrome_y);
    size.x = size.x < max_width ? size.x : max_width;
    size.y = size.y < max_height ? size.y : max_height;
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    char title[200] = {};
    std::snprintf(title, sizeof(title), "%s###image_settings", piccolo_text(context->strings, Str::image_title));
    if (ImGui::Begin(title, nullptr, flags))
    {
        retro_ui_begin_window(scale);
        if (retro_ui_close_button())
        {
            *open = false;
        }
        ImGui::TextDisabled("%s: %s", piccolo_text(context->strings, Str::image_editing),
                            settings_ui_scope_name(context->strings, context->scope));

        if (has_picture)
        {
            draw_preview(context, source, factor, block);
        }
        else
        {
            ImGui::BeginGroup();
            ImGui::TextDisabled("%s", piccolo_text(context->strings, Str::image_no_picture));
            ImGui::Dummy(ImVec2(picture.x, 0.0f));
            ImGui::EndGroup();
        }

        // A child region, so the rows (and their RESET buttons) fit the column.
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        if (ImGui::BeginChild("controls", ImVec2(0.0f, body_height)))
        {
            // A child has its own draw list: the pixel font needs nearest sampling here too.
            const ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
            ImGui::GetWindowDrawList()->AddCallback(platform_io.DrawCallback_SetSamplerNearest, nullptr);
            draw_controls(context);
            ImGui::GetWindowDrawList()->AddCallback(platform_io.DrawCallback_SetSamplerLinear, nullptr);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        retro_ui_end_window();
    }
    ImGui::End();
    retro_ui_pop_theme();
}
