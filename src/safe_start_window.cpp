#include "safe_start_window.h"

#include <cstdio>

#include "imgui.h"

#include "piccolo_text.h"
#include "retro_ui.h"

SafeStartChoice safe_start_window_draw(ImFont* pixel_font, int32_t scale, const Localization* strings, const char* core,
                                       bool options_changed)
{
    SafeStartChoice choice = SafeStartChoice::none;
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
    std::snprintf(title, sizeof(title), "%s###safe_start", piccolo_text(strings, Str::safe_start_title));
    if (ImGui::Begin(title, nullptr, flags))
    {
        retro_ui_begin_window(scale);
        if (retro_ui_close_button())
        {
            choice = SafeStartChoice::cancel;
        }
        ImGui::PushTextWrapPos(0.0f);
        ImGui::Text("%s %s", core, piccolo_text(strings, Str::safe_start_crashed));
        if (options_changed)
        {
            ImGui::TextUnformatted(piccolo_text(strings, Str::safe_start_after_options));
        }
        ImGui::TextUnformatted(piccolo_text(strings, Str::safe_start_question));
        ImGui::PopTextWrapPos();
        ImGui::Spacing();

        if (ImGui::Button(piccolo_text(strings, Str::safe_start_stored)))
        {
            choice = SafeStartChoice::stored;
        }
        ImGui::SameLine();
        if (ImGui::Button(piccolo_text(strings, Str::safe_start_defaults)))
        {
            choice = SafeStartChoice::defaults;
        }
        retro_ui_item_tooltip(piccolo_text(strings, Str::safe_start_defaults_help));
        if (ImGui::Button(piccolo_text(strings, Str::safe_start_delete)))
        {
            choice = SafeStartChoice::delete_stored;
        }
        retro_ui_item_tooltip(piccolo_text(strings, Str::safe_start_delete_help));
        ImGui::SameLine();
        if (ImGui::Button(piccolo_text(strings, Str::safe_start_cancel)))
        {
            choice = SafeStartChoice::cancel;
        }
        retro_ui_end_window();
    }
    ImGui::End();
    retro_ui_pop_theme();
    return choice;
}
