#include "cores_window.h"

#include <cstdio>
#include <string>

#include "imgui.h"
#include "piccolo_text.h"
#include "retro_ui.h"

void cores_window_open(CoresWindow* window, CoreUpdater* updater)
{
    window->open = true;
    window->selected = -1;
    window->update_all = false;
    core_updater_refresh(updater);
}

static const char* status_text(const Localization* strings, CoreStatus status)
{
    switch (status)
    {
        case CoreStatus::not_installed:     return piccolo_text(strings, Str::cores_status_not_installed);
        case CoreStatus::installed:         return piccolo_text(strings, Str::cores_status_installed);
        case CoreStatus::update:            return piccolo_text(strings, Str::cores_status_update);
        case CoreStatus::installed_by_hand: return piccolo_text(strings, Str::cores_status_by_hand);
    }
    return "";
}

// "Game Boy|Game Boy Color" -> "Game Boy, Game Boy Color".
static std::string systems_text(const std::string& systems)
{
    std::string text;
    for (size_t i = 0; i < systems.size(); i++)
    {
        if (systems[i] == '|')
        {
            text += ", ";
        }
        else
        {
            text.push_back(systems[i]);
        }
    }
    return text;
}

static void size_text(int64_t bytes, char* out, size_t out_size)
{
    if (bytes >= 1024 * 1024)
    {
        std::snprintf(out, out_size, "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    }
    else
    {
        std::snprintf(out, out_size, "%lld KB", static_cast<long long>((bytes + 1023) / 1024));
    }
}

// UPDATE ALL: whenever the downloader is idle, the next core with an update
// (stops at the first error, or when none is left).
static void continue_update_all(CoresWindow* window, const CoresWindowContext* context)
{
    CoreUpdater* updater = context->updater;
    if (!window->update_all || updater->state != CoreUpdaterState::idle)
    {
        return;
    }
    if (updater->message_is_error)
    {
        window->update_all = false;
        return;
    }
    for (size_t i = 0; i < updater->index.entries.size(); i++)
    {
        int32_t entry = static_cast<int32_t>(i);
        if (core_updater_status(updater, entry) == CoreStatus::update &&
            updater->index.entries[i].file != context->running_file &&
            (!updater->free_only || updater->index.entries[i].free))
        {
            core_updater_install(updater, entry, context->running_file);
            return;
        }
    }
    window->update_all = false;
}

static void draw_list(CoresWindow* window, const CoresWindowContext* context, float block, float height)
{
    CoreUpdater* updater = context->updater;
    const Localization* strings = context->strings;
    retro_ui_begin_list("cores", 0.0f, height);
    // A gap between columns, so a long SYSTEMS text doesn't run into VERSION.
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f * block, ImGui::GetStyle().CellPadding.y));
    if (ImGui::BeginTable("cores_table", 4, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn(piccolo_text(strings, Str::cores_column_core), ImGuiTableColumnFlags_WidthFixed, 80.0f * block);
        ImGui::TableSetupColumn(piccolo_text(strings, Str::cores_column_systems), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(piccolo_text(strings, Str::cores_column_version), ImGuiTableColumnFlags_WidthFixed, 50.0f * block);
        ImGui::TableSetupColumn(piccolo_text(strings, Str::cores_column_status), ImGuiTableColumnFlags_WidthFixed, 60.0f * block);
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < updater->index.entries.size(); i++)
        {
            const CoreIndexEntry* entry = &updater->index.entries[i];
            int32_t index = static_cast<int32_t>(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            char label[300];
            std::snprintf(label, sizeof(label), "%s%s###core%d", entry->name.c_str(), entry->free ? "" : " *",
                          static_cast<int>(index));
            if (ImGui::Selectable(label, window->selected == index, ImGuiSelectableFlags_SpanAllColumns))
            {
                window->selected = index;
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(systems_text(entry->systems).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(entry->version.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(status_text(strings, core_updater_status(updater, index)));
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    retro_ui_end_list();
}

static void draw_selected(CoresWindow* window, const CoresWindowContext* context)
{
    CoreUpdater* updater = context->updater;
    const Localization* strings = context->strings;
    bool busy = updater->state != CoreUpdaterState::idle;
    if (window->selected < 0 || window->selected >= static_cast<int32_t>(updater->index.entries.size()))
    {
        ImGui::TextDisabled("%s", piccolo_text(strings, updater->index.entries.empty() ? Str::cores_empty
                                                                                         : Str::cores_none_selected));
        return;
    }
    const CoreIndexEntry* entry = &updater->index.entries[static_cast<size_t>(window->selected)];
    CoreStatus status = core_updater_status(updater, window->selected);
    char size[32];
    size_text(entry->size, size, sizeof(size));
    ImGui::Text("%s %s (%s)   %s %s", piccolo_text(strings, Str::cores_license), entry->license.c_str(),
                piccolo_text(strings, entry->free ? Str::cores_free : Str::cores_non_free),
                piccolo_text(strings, Str::cores_size), size);
    if (status == CoreStatus::installed || status == CoreStatus::update)
    {
        ImGui::SameLine();
        ImGui::Text("   %s %s", piccolo_text(strings, Str::cores_installed_version),
                    core_updater_installed_version(updater, entry->file.c_str()).c_str());
    }
    ImGui::PushTextWrapPos(0.0f);
    if (!entry->free)
    {
        ImGui::TextDisabled("%s", piccolo_text(strings, Str::cores_non_free_note));
    }
    if (status == CoreStatus::installed_by_hand)
    {
        ImGui::TextDisabled("%s", piccolo_text(strings, Str::cores_by_hand_note));
    }
    ImGui::PopTextWrapPos();

    ImGui::BeginDisabled(busy);
    if (status != CoreStatus::installed)
    {
        Str action = Str::cores_install;
        if (status == CoreStatus::update)
        {
            action = Str::cores_update;
        }
        else if (status == CoreStatus::installed_by_hand)
        {
            action = Str::cores_replace;
        }
        if (retro_ui_button(piccolo_text(strings, action)))
        {
            core_updater_install(updater, window->selected, context->running_file);
        }
        ImGui::SameLine();
    }
    if (status != CoreStatus::not_installed && retro_ui_button(piccolo_text(strings, Str::cores_remove)))
    {
        core_updater_remove(updater, window->selected, context->running_file);
    }
    ImGui::EndDisabled();
}

void cores_window_draw(CoresWindow* window, const CoresWindowContext* context, ImFont* pixel_font, int32_t scale)
{
    if (!window->open)
    {
        return;
    }
    CoreUpdater* updater = context->updater;
    const Localization* strings = context->strings;
    continue_update_all(window, context);

    float block = static_cast<float>(scale);
    const ImGuiIO& io = ImGui::GetIO();
    retro_ui_push_theme(pixel_font, scale);
    // The same size as SETTINGS: 360 x 240 retro pixels, never bigger than the window.
    ImVec2 size(360.0f * block, 240.0f * block);
    size.x = size.x < io.DisplaySize.x - 8.0f * block ? size.x : io.DisplaySize.x - 8.0f * block;
    size.y = size.y < io.DisplaySize.y - 24.0f * block ? size.y : io.DisplaySize.y - 24.0f * block;
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    char title[200] = {};
    std::snprintf(title, sizeof(title), "%s###cores_window", piccolo_text(strings, Str::cores_title));
    if (ImGui::Begin(title, nullptr, flags))
    {
        retro_ui_begin_window(scale);
        if (retro_ui_close_button())
        {
            window->open = false;
        }
        if (!core_updater_available())
        {
            ImGui::TextDisabled("%s", piccolo_text(strings, Str::cores_unavailable));
        }
        else
        {
            // The list takes what's left above the details, buttons and status,
            // measured last frame (their height changes with notes and buttons;
            // the first frame guesses 4 lines).
            float line = ImGui::GetFrameHeightWithSpacing();
            float bottom = window->bottom_height > 0.0f ? window->bottom_height : line * 4.0f;
            float list_height = ImGui::GetContentRegionAvail().y - bottom;
            draw_list(window, context, block, list_height > line * 3.0f ? list_height : line * 3.0f);
            float bottom_start = ImGui::GetCursorPosY();
            draw_selected(window, context);

            bool busy = updater->state != CoreUpdaterState::idle;
            ImGui::BeginDisabled(busy);
            if (retro_ui_button(piccolo_text(strings, Str::cores_refresh)))
            {
                window->update_all = false;
                core_updater_refresh(updater);
            }
            ImGui::SameLine();
            if (retro_ui_button(piccolo_text(strings, Str::cores_update_all)))
            {
                window->update_all = true;
                updater->message_is_error = false;   // a new run: don't stop on the last error
            }
            ImGui::EndDisabled();
            if (busy)
            {
                ImGui::SameLine();
                if (retro_ui_button(piccolo_text(strings, Str::cores_cancel)))
                {
                    window->update_all = false;
                    core_updater_cancel(updater);
                }
            }

            // Status: the download's progress, or what the downloader did last.
            int64_t received = 0;
            int64_t total = 0;
            core_updater_progress(updater, &received, &total);
            if (updater->state == CoreUpdaterState::installing && total > 0)
            {
                ImGui::Text("%s %s %d%%", piccolo_text(strings, Str::cores_downloading), updater->task_entry.name.c_str(),
                            static_cast<int>(received * 100 / total));
            }
            else if (updater->message_is_error)
            {
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(retro_ui_attention_color()), "%s",
                                   updater->message.c_str());
            }
            else
            {
                ImGui::TextDisabled("%s", updater->message.c_str());
            }
            window->bottom_height = ImGui::GetCursorPosY() - bottom_start;
        }
        retro_ui_end_window();
    }
    ImGui::End();
    retro_ui_pop_theme();
}
