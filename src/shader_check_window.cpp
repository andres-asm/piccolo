#include "shader_check_window.h"

#include <cstdio>

#include "imgui.h"

#include "piccolo_text.h"
#include "retro_ui.h"

ShaderCheckChoice shader_check_window_draw(ImFont* pixel_font, int32_t scale, const Localization* strings,
                                           const char* preset, double measured_fps, double target_fps,
                                           uint64_t asked_ms, uint64_t now_ms)
{
    ShaderCheckChoice choice = ShaderCheckChoice::none;
    float block = static_cast<float>(scale);
    const ImGuiIO& io = ImGui::GetIO();

    retro_ui_push_theme(pixel_font, scale);
    // 300 retro pixels wide (never wider than the window), as tall as its text.
    float width = 300.0f * block;
    width = width < io.DisplaySize.x - 8.0f * block ? width : io.DisplaySize.x - 8.0f * block;
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowFocus();
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize;
    char title[200] = {};
    std::snprintf(title, sizeof(title), "%s###shader_check", piccolo_text(strings, Str::shader_slow_title));
    if (ImGui::Begin(title, nullptr, flags))
    {
        retro_ui_begin_window(scale);
        if (retro_ui_close_button())
        {
            choice = ShaderCheckChoice::revert;
        }
        ImGui::PushTextWrapPos(0.0f);
        char speed[200] = {};
        std::snprintf(speed, sizeof(speed), piccolo_text(strings, Str::shader_slow_text), measured_fps, target_fps);
        ImGui::Text("%s %s", preset, speed);
        ImGui::TextUnformatted(piccolo_text(strings, Str::shader_slow_question));
        ImGui::PopTextWrapPos();
        ImGui::Spacing();

        // KEEP and REVERT at the right; REVERT counts down.
        int seconds = retro_ui_seconds_left(now_ms, asked_ms, SHADER_CHECK_ANSWER_MS);
        const char* keep = piccolo_text(strings, Str::shader_slow_keep);
        char revert[128] = {};
        std::snprintf(revert, sizeof(revert), piccolo_text(strings, Str::shader_slow_revert_n), seconds);
        float padding = ImGui::GetStyle().FramePadding.x * 2.0f;
        float right = ImGui::CalcTextSize(keep).x + ImGui::CalcTextSize(revert).x + padding * 2.0f +
                      ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - right);
        ImGui::PushID("keep");
        bool keep_clicked = retro_ui_button(keep);
        ImGui::PopID();
        if (keep_clicked)
        {
            choice = ShaderCheckChoice::keep;
        }
        ImGui::SameLine();
        if (retro_ui_countdown_button(piccolo_text(strings, Str::shader_slow_revert_n), seconds, "revert") ||
            now_ms - asked_ms >= SHADER_CHECK_ANSWER_MS)
        {
            choice = ShaderCheckChoice::revert;
        }
        retro_ui_end_window();
    }
    ImGui::End();
    retro_ui_pop_theme();
    return choice;
}
