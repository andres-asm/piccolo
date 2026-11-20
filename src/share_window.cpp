#include "share_window.h"

#include <cfloat>
#include <cstdio>
#include <ctime>

#include "imgui.h"
#include "piccolo_text.h"
#include "retro_ui.h"

void share_window_open(ShareWindow* window)
{
    window->open = true;
    window->local.stale = true;
    window->remote.stale = true;
    window->remote.waiting = false;
    window->cancel = false;
}

void share_window_remote_reset(ShareWindow* window)
{
    window->remote = SharePane();
    window->remote_roots.clear();
}

bool share_window_remote_wants_list(ShareWindow* window, std::string* path)
{
    if (!window->open || !window->remote.stale || window->remote.waiting)
    {
        return false;
    }
    window->remote.stale = false;
    window->remote.waiting = true;
    *path = window->remote.path;
    return true;
}

void share_window_set_remote_list(ShareWindow* window, const char* path, NetShareStatus status,
                                  const std::vector<NetShareEntry>* entries)
{
    SharePane* pane = &window->remote;
    if (pane->path != path)
    {
        return;   // an answer for a folder we've left
    }
    pane->waiting = false;
    pane->status = status;
    pane->entries = *entries;
    pane->selected = -1;
    if (pane->path.empty() && status == NetShareStatus::ok)
    {
        window->remote_roots = *entries;   // its shared folders, for the combo while it's browsed further in
    }
}

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------

// The shared folder a virtual path starts with ("ROMS/gba" -> "ROMS").
static std::string root_of(const std::string* path)
{
    size_t slash = path->find('/');
    return slash == std::string::npos ? *path : path->substr(0, slash);
}

static std::string child(const std::string* path, const std::string* name)
{
    return path->empty() ? *name : *path + "/" + *name;
}

static void go_up(SharePane* pane)
{
    size_t slash = pane->path.rfind('/');
    pane->path = slash == std::string::npos ? std::string() : pane->path.substr(0, slash);
    pane->stale = true;
    pane->selected = -1;
}

static void go_to(SharePane* pane, const std::string* path)
{
    pane->path = *path;
    pane->stale = true;
    pane->selected = -1;
}

// The kind of the shared folder `path` is in (games, saves, states), from
// the root listing; games when it isn't known.
static NetShareKind root_kind(const std::vector<NetShareEntry>* roots, const std::string* path)
{
    std::string root = root_of(path);
    for (size_t i = 0; i < roots->size(); i++)
    {
        if ((*roots)[i].name == root)
        {
            return (*roots)[i].kind;
        }
    }
    return NetShareKind::games;
}

static bool is_folder(NetShareKind kind)
{
    return kind != NetShareKind::file;
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------

static void format_size(uint64_t bytes, char* out, size_t size)
{
    if (bytes >= 1024ull * 1024 * 1024)
    {
        std::snprintf(out, size, "%.1f GIB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    }
    else if (bytes >= 1024ull * 1024)
    {
        std::snprintf(out, size, "%.1f MIB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    }
    else if (bytes >= 1024)
    {
        std::snprintf(out, size, "%llu KIB", static_cast<unsigned long long>(bytes / 1024));
    }
    else
    {
        std::snprintf(out, size, "%llu B", static_cast<unsigned long long>(bytes));
    }
}

// Saves and states are told apart by when they were made: the date.
static void format_date(uint64_t mtime, char* out, size_t size)
{
    if (mtime == 0)
    {
        std::snprintf(out, size, "-");
        return;
    }
    std::time_t time = static_cast<std::time_t>(mtime);
    std::tm local = {};
#if defined(_WIN32)
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    std::strftime(out, size, "%Y-%m-%d %H:%M", &local);
}

// ---------------------------------------------------------------------------
// One side
// ---------------------------------------------------------------------------

struct PaneLayout
{
    float width = 0.0f;
    float folders_height = 0.0f;
    float files_height = 0.0f;
};

static void draw_pane(SharePane* pane, std::vector<NetShareEntry>* roots, const char* name, bool connected,
                      const PaneLayout* layout, const Localization* strings)
{
    ImGui::BeginGroup();
    ImGui::PushID(name);
    ImGui::TextUnformatted(name);

    // The shared folder we're in.
    std::string root = root_of(&pane->path);
    const char* preview = root.empty() ? piccolo_text(strings, Str::share_shared_folders) : root.c_str();
    ImGui::SetNextItemWidth(layout->width);
    if (retro_ui_begin_combo("##root", preview))
    {
        for (size_t i = 0; i < roots->size(); i++)
        {
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable((*roots)[i].name.c_str(), (*roots)[i].name == root))
            {
                go_to(pane, &(*roots)[i].name);
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::TextDisabled("%s", pane->path.empty() ? " " : pane->path.c_str());

    bool listed = connected && !pane->waiting && pane->status == NetShareStatus::ok;

    // Folders (at the top: the shared folders themselves).
    std::string open_folder;
    bool up = false;
    retro_ui_begin_list("folders", layout->width, layout->folders_height);
    if (!connected)
    {
        ImGui::TextDisabled("%s", piccolo_text(strings, Str::share_not_connected));
    }
    else if (pane->waiting)
    {
        ImGui::TextDisabled("...");
    }
    else
    {
        if (!pane->path.empty() && ImGui::Selectable(".."))
        {
            up = true;
        }
        for (size_t i = 0; listed && i < pane->entries.size(); i++)
        {
            if (!is_folder(pane->entries[i].kind))
            {
                continue;
            }
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable(pane->entries[i].name.c_str()))
            {
                open_folder = pane->entries[i].name;
            }
            ImGui::PopID();
        }
        if (pane->status != NetShareStatus::ok)
        {
            ImGui::TextDisabled("%s", piccolo_text(strings, Str::share_list_failed));
        }
    }
    retro_ui_end_list();

    // Files below: name, and the size (games) or the date (saves, states).
    bool by_date = root_kind(roots, &pane->path) != NetShareKind::games;
    retro_ui_begin_list("files", layout->width, layout->files_height);
    bool any_file = false;
    for (size_t i = 0; listed && i < pane->entries.size(); i++)
    {
        const NetShareEntry* entry = &pane->entries[i];
        if (entry->kind != NetShareKind::file)
        {
            continue;
        }
        any_file = true;
        ImGui::PushID(static_cast<int>(i));
        float row_x = ImGui::GetCursorPosX();
        if (ImGui::Selectable(entry->name.c_str(), pane->selected == static_cast<int32_t>(i)))
        {
            pane->selected = static_cast<int32_t>(i);
        }
        char detail[48] = {};
        if (by_date)
        {
            format_date(entry->mtime, detail, sizeof(detail));
        }
        else
        {
            format_size(entry->size, detail, sizeof(detail));
        }
        float detail_width = ImGui::CalcTextSize(detail).x;
        ImGui::SameLine(row_x + ImGui::GetContentRegionAvail().x - detail_width);
        ImGui::TextUnformatted(detail);
        ImGui::PopID();
    }
    if (listed && !any_file && !pane->path.empty())
    {
        ImGui::TextDisabled("%s", piccolo_text(strings, Str::share_no_files));
    }
    retro_ui_end_list();

    ImGui::PopID();
    ImGui::EndGroup();

    if (up)
    {
        go_up(pane);
    }
    else if (!open_folder.empty())
    {
        std::string next = child(&pane->path, &open_folder);
        go_to(pane, &next);
    }
}

static Str refusal_text(NetClientRefusal refusal)
{
    switch (refusal)
    {
    case NetClientRefusal::not_paired:
        return Str::share_refused_not_paired;
    case NetClientRefusal::pairing_closed:
        return Str::share_refused_pairing_closed;
    case NetClientRefusal::wrong_code:
        return Str::share_refused_wrong_code;
    case NetClientRefusal::unknown_host:
        return Str::share_refused_unknown_host;
    default:
        return Str::share_refused_connection;
    }
}

// The other side while no piccolo is connected with OWNER access: the
// piccolos on this network, the code (only the first time) and CONNECT; or,
// connected at a lower level, what to ask for there.
static void draw_connect(ShareWindow* window, const NetShareLinkStatus* link, const PaneLayout* layout,
                         const Localization* strings)
{
    ImGui::BeginGroup();
    ImGui::PushID("connect");
    ImGui::TextUnformatted(piccolo_text(strings, Str::share_other_piccolo));
    float height = layout->folders_height + layout->files_height + ImGui::GetStyle().ItemSpacing.y +
                   ImGui::GetFrameHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing();
    retro_ui_begin_list("connect_box", layout->width, height);
    ImGui::PushTextWrapPos(0.0f);
    if (link == nullptr || link->state == NetShareLinkState::off)
    {
        ImGui::TextDisabled("%s", piccolo_text(strings, Str::share_network_off));
    }
    else if (link->state == NetShareLinkState::ready)
    {
        // Connected, but not at OWNER: the other piccolo decides that.
        ImGui::TextUnformatted(link->host_name.c_str());
        ImGui::TextDisabled("%s", piccolo_text(strings, Str::share_needs_owner));
        if (retro_ui_button(piccolo_text(strings, Str::share_disconnect)))
        {
            window->disconnect = true;
        }
    }
    else
    {
        ImGui::TextDisabled("%s", piccolo_text(strings, Str::share_found));
        if (window->found_selected >= static_cast<int32_t>(link->found.size()))
        {
            window->found_selected = -1;
        }
        for (size_t i = 0; i < link->found.size(); i++)
        {
            ImGui::PushID(static_cast<int>(i));
            char address[64] = {};
            net_address_to_text(link->found[i].address, address, sizeof(address));
            char label[160] = {};
            std::snprintf(label, sizeof(label), "%s  %s", link->found[i].name.c_str(), address);
            if (ImGui::Selectable(label, window->found_selected == static_cast<int32_t>(i)))
            {
                window->found_selected = static_cast<int32_t>(i);
            }
            ImGui::PopID();
        }
        if (link->found.empty())
        {
            ImGui::TextDisabled("%s", piccolo_text(strings, Str::share_found_none));
        }
        ImGui::Spacing();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(piccolo_text(strings, Str::share_code));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("0000000").x + ImGui::GetStyle().FramePadding.x * 2.0f);
        retro_ui_push_field_text();
        ImGui::InputText("##code", window->code, sizeof(window->code), ImGuiInputTextFlags_CharsDecimal);
        retro_ui_pop_field_text();
        retro_ui_item_tooltip(piccolo_text(strings, Str::share_code_help));
        bool connecting = link->state == NetShareLinkState::connecting;
        ImGui::BeginDisabled(window->found_selected < 0 || connecting);
        if (retro_ui_button(piccolo_text(strings, Str::share_connect)))
        {
            window->connect = true;
        }
        ImGui::EndDisabled();
        if (connecting)
        {
            ImGui::TextDisabled("%s", piccolo_text(strings, Str::share_connecting));
        }
        else if (link->refusal != NetClientRefusal::none)
        {
            ImGui::TextDisabled("%s", piccolo_text(strings, refusal_text(link->refusal)));
        }
    }
    ImGui::PopTextWrapPos();
    retro_ui_end_list();
    ImGui::PopID();
    ImGui::EndGroup();
}

static const NetShareEntry* selected_file(const SharePane* pane)
{
    if (pane->selected < 0 || static_cast<size_t>(pane->selected) >= pane->entries.size())
    {
        return nullptr;
    }
    const NetShareEntry* entry = &pane->entries[static_cast<size_t>(pane->selected)];
    return entry->kind == NetShareKind::file ? entry : nullptr;
}

// An arrow copies the selected file from `from` into the folder open in `to`.
static void draw_arrow(ShareWindow* window, bool to_remote, bool connected, float width, float height,
                       const Localization* strings)
{
    const SharePane* from = to_remote ? &window->local : &window->remote;
    const SharePane* to = to_remote ? &window->remote : &window->local;
    const NetShareEntry* file = selected_file(from);
    bool can = connected && file != nullptr && !to->path.empty() && !to->waiting;
    ImGui::BeginDisabled(!can);
    bool clicked = retro_ui_arrow_button(to_remote ? "##to_remote" : "##to_local",
                                         to_remote ? RetroUiArrow::right : RetroUiArrow::left, width, height);
    ImGui::EndDisabled();
    retro_ui_item_tooltip(piccolo_text(strings, to_remote ? Str::share_copy_to_remote_help : Str::share_copy_to_local_help));
    if (clicked && can)
    {
        ShareCopy copy;
        copy.to_remote = to_remote;
        copy.from = child(&from->path, &file->name);
        copy.to = child(&to->path, &file->name);
        copy.size = file->size;
        window->copies.push_back(copy);
    }
}

// The bottom line: the copy running, or what the arrows do.
static void draw_line(ShareWindow* window, const ShareTransferLine* line, bool connected, const Localization* strings,
                      float block)
{
    if (line == nullptr || !line->active)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", window->message.empty() ? piccolo_text(strings, Str::share_hint) : window->message.c_str());
        if (connected)
        {
            const char* label = piccolo_text(strings, Str::share_disconnect);
            float width = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - width);
            if (retro_ui_button(label))
            {
                window->disconnect = true;
            }
        }
        return;
    }
    const char* verb = piccolo_text(strings, line->sending ? Str::share_sending : Str::share_receiving);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s %s", verb, line->name.c_str());
    ImGui::SameLine();

    // The bar: the field color, filled with the highlight as far as it's done.
    float bar_width = 80.0f * block;
    float bar_height = 6.0f * block;
    ImVec2 at = ImGui::GetCursorScreenPos();
    at.y += (ImGui::GetFrameHeight() - bar_height) * 0.5f;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(at, ImVec2(at.x + bar_width, at.y + bar_height), ImGui::GetColorU32(ImGuiCol_FrameBg));
    float done = line->size > 0 ? static_cast<float>(static_cast<double>(line->done) / static_cast<double>(line->size)) : 1.0f;
    draw_list->AddRectFilled(at, ImVec2(at.x + bar_width * done, at.y + bar_height),
                             ImGui::GetColorU32(ImGuiCol_HeaderActive));
    ImGui::Dummy(ImVec2(bar_width, ImGui::GetFrameHeight()));
    ImGui::SameLine();

    char done_text[32] = {};
    char size_text[32] = {};
    format_size(line->done, done_text, sizeof(done_text));
    format_size(line->size, size_text, sizeof(size_text));
    if (line->paused)
    {
        ImGui::Text("%s / %s  %s", done_text, size_text, piccolo_text(strings, Str::share_paused));
    }
    else
    {
        ImGui::Text("%s / %s  %.1f MBIT/S", done_text, size_text, static_cast<double>(line->rate_kbps) / 1000.0);
    }
    if (line->queued > 0)
    {
        ImGui::SameLine();
        ImGui::Text("+%u", line->queued);
    }
    ImGui::SameLine();
    const char* cancel = piccolo_text(strings, Str::share_cancel);
    float cancel_width = ImGui::CalcTextSize(cancel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - cancel_width);
    if (retro_ui_button(cancel))
    {
        window->cancel = true;
    }
}

// ---------------------------------------------------------------------------
// The window
// ---------------------------------------------------------------------------

void share_window_draw(ShareWindow* window, const ShareWindowContext* context, ImFont* pixel_font, int32_t scale)
{
    if (!window->open)
    {
        return;
    }
    const Localization* strings = context->strings;
    const NetShareLinkStatus* link = context->link;
    bool connected = link != nullptr && link->state == NetShareLinkState::ready && link->access == NetAccessLevel::owner;
    if (window->local.stale && context->local_folders != nullptr)
    {
        window->local.stale = false;
        net_share_list(context->local_folders, "", &window->local_roots);
        window->local.status = net_share_list(context->local_folders, window->local.path.c_str(), &window->local.entries);
        window->local.selected = -1;
        if (window->local.status != NetShareStatus::ok && !window->local.path.empty())
        {
            std::string top;
            go_to(&window->local, &top);   // gone meanwhile: back to the shared folders
        }
    }

    float block = static_cast<float>(scale);
    const ImGuiIO& io = ImGui::GetIO();
    retro_ui_push_theme(pixel_font, scale);
    // 560 x 300 retro pixels, never bigger than the window.
    ImVec2 size(560.0f * block, 300.0f * block);
    size.x = size.x < io.DisplaySize.x - 8.0f * block ? size.x : io.DisplaySize.x - 8.0f * block;
    size.y = size.y < io.DisplaySize.y - 24.0f * block ? size.y : io.DisplaySize.y - 24.0f * block;
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));
    bool keep_open = true;
    char title[128] = {};
    std::snprintf(title, sizeof(title), "%s###share", piccolo_text(strings, Str::share_title));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin(title, nullptr, flags))
    {
        retro_ui_begin_window(scale);
        if (retro_ui_close_button())
        {
            keep_open = false;
        }
        // Two sides with the arrows between them; two rows left at the bottom.
        float middle = 30.0f * block;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        PaneLayout layout;
        layout.width = static_cast<float>(static_cast<int32_t>((ImGui::GetContentRegionAvail().x - middle - spacing * 2.0f) * 0.5f));
        float lists = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2.0f -
                      ImGui::GetTextLineHeightWithSpacing() * 2.0f - ImGui::GetFrameHeightWithSpacing() * 2.0f;
        layout.folders_height = static_cast<float>(static_cast<int32_t>(lists * 0.38f));
        layout.files_height = static_cast<float>(static_cast<int32_t>(lists - layout.folders_height));

        draw_pane(&window->local, &window->local_roots, piccolo_text(strings, Str::share_this_piccolo), true, &layout, strings);

        ImGui::SameLine();
        ImGui::BeginGroup();
        // The arrows, about level with where the two lists meet.
        float top = ImGui::GetFrameHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing() * 2.0f +
                    layout.folders_height - 9.0f * block;
        ImGui::Dummy(ImVec2(middle, top));
        float arrow_width = 22.0f * block;
        float arrow_height = 18.0f * block;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (middle - arrow_width) * 0.5f);
        draw_arrow(window, true, connected, arrow_width, arrow_height, strings);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (middle - arrow_width) * 0.5f);
        draw_arrow(window, false, connected, arrow_width, arrow_height, strings);
        ImGui::EndGroup();
        ImGui::SameLine();

        if (connected)
        {
            draw_pane(&window->remote, &window->remote_roots, link->host_name.c_str(), true, &layout, strings);
        }
        else
        {
            draw_connect(window, link, &layout, strings);
        }

        draw_line(window, context->line, connected, strings, block);
        retro_ui_end_window();
    }
    ImGui::End();
    retro_ui_pop_theme();
    if (!keep_open)
    {
        window->open = false;
    }
}
