#include "file_picker.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <SDL3/SDL.h>

#include "content_archive.h"
#include "host_fs.h"
#include "imgui.h"

#include "piccolo_text.h"
#include "retro_ui.h"

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------

// "a.7z#" (an archive's top folder) takes names right after the '#'.
static bool join_path(const char* directory, const char* name, char* out, size_t out_size)
{
    size_t length = std::strlen(directory);
    std::string archive;
    std::string folder;
    bool archive_top = archive_split_path(directory, &archive, &folder) && folder.empty();
    bool has_separator = archive_top || (length > 0 && (directory[length - 1] == '/' || directory[length - 1] == '\\'));
    int written = std::snprintf(out, out_size, "%s%s%s", directory, has_separator ? "" : "/", name);
    return written > 0 && static_cast<size_t>(written) < out_size;
}

static bool is_separator(char c)
{
    return c == '/' || c == '\\';
}

// "/a/b/c" -> "/a/b", "/a" -> "/". Stays "/" at the root.
// Windows drives: "C:/a" -> "C:/" (not "C:", which means the drive's current
// folder), and it stays at "C:/".
// Archives: "/r/a.7z#d/e" -> "/r/a.7z#d" -> "/r/a.7z#" -> "/r".
static void go_to_parent(char* directory)
{
    std::string archive;
    std::string folder;
    if (archive_split_path(directory, &archive, &folder))
    {
        // Either way the result is shorter than `directory`: copied in place.
        if (!folder.empty())
        {
            size_t slash = folder.rfind('/');
            std::string parent = archive + "#" + (slash == std::string::npos ? std::string() : folder.substr(0, slash));
            std::memcpy(directory, parent.c_str(), parent.size() + 1);
            return;
        }
        std::memcpy(directory, archive.c_str(), archive.size() + 1);   // then its folder's parent, below
    }
    size_t length = std::strlen(directory);
    bool drive = length >= 2 && directory[1] == ':' &&
                 ((directory[0] >= 'A' && directory[0] <= 'Z') || (directory[0] >= 'a' && directory[0] <= 'z'));
    if (drive)
    {
        size_t last_separator = 0;
        for (size_t i = 2; i + 1 < length; i++)   // a trailing separator doesn't count
        {
            if (is_separator(directory[i]))
            {
                last_separator = i;
            }
        }
        if (last_separator <= 2)
        {
            directory[2] = '/';   // the drive's root
            directory[3] = '\0';
            return;
        }
    }
    // Ignore trailing separators ("/a/b/" is "/a/b").
    while (length > 1 && is_separator(directory[length - 1]))
    {
        length--;
    }
    // Find the separator before the last component.
    size_t last = length;
    while (last > 0 && !is_separator(directory[last - 1]))
    {
        last--;
    }
    if (last <= 1)
    {
        std::snprintf(directory, 2, "/");   // the parent is the root
        return;
    }
    directory[last - 1] = '\0';
}

// ---------------------------------------------------------------------------
// Listing
// ---------------------------------------------------------------------------

static int compare_names(const void* a, const void* b)
{
    return SDL_strcasecmp(static_cast<const char*>(a), static_cast<const char*>(b));
}

static const LibretroCoreInfo* selected_core(const FilePicker* picker, const CoreCatalog* catalog)
{
    if (picker->core_index < 0 || picker->core_index >= catalog->count)
    {
        return nullptr;
    }
    return &catalog->entries[picker->core_index].info;
}

static void add_folder(FilePicker* picker, const char* name)
{
    if (picker->folder_count < FILE_PICKER_MAX_ENTRIES && std::strlen(name) < FILE_PICKER_NAME_MAX)
    {
        std::snprintf(picker->folders[picker->folder_count], FILE_PICKER_NAME_MAX, "%s", name);
        picker->folder_count++;
    }
}

static void add_file(FilePicker* picker, const char* name)
{
    if (picker->file_count < FILE_PICKER_MAX_ENTRIES && std::strlen(name) < FILE_PICKER_NAME_MAX)
    {
        std::snprintf(picker->files[picker->file_count], FILE_PICKER_NAME_MAX, "%s", name);
        picker->file_count++;
    }
}

// Archives list folders once per file inside them ("d/a", "d/b"): added once.
static void add_folder_once(FilePicker* picker, const char* name)
{
    for (int32_t i = 0; i < picker->folder_count; i++)
    {
        if (std::strcmp(picker->folders[i], name) == 0)
        {
            return;
        }
    }
    add_folder(picker, name);
}

// Hidden entries, and the resource folder macOS adds to zips it makes.
static bool hidden_name(const char* name)
{
    return name[0] == '.' || std::strcmp(name, "__MACOSX") == 0;
}

// The folder `folder` ("" = the top) inside the archive: its subfolders and
// the files the core can load.
static void list_archive(FilePicker* picker, const LibretroCoreInfo* core, const std::string* archive,
                         const std::string* folder)
{
    if (picker->archive_path != *archive)
    {
        archive_list(archive->c_str(), &picker->archive_entries);   // a failure leaves it empty (logged)
        picker->archive_path = *archive;
    }
    std::string prefix = folder->empty() ? std::string() : *folder + "/";
    for (size_t i = 0; i < picker->archive_entries.size(); i++)
    {
        const ArchiveEntry* entry = &picker->archive_entries[i];
        if (entry->name.size() <= prefix.size() || entry->name.compare(0, prefix.size(), prefix) != 0)
        {
            continue;   // not inside this folder (or the folder itself)
        }
        std::string rest = entry->name.substr(prefix.size());
        size_t slash = rest.find('/');
        std::string first = slash == std::string::npos ? rest : rest.substr(0, slash);
        if (hidden_name(first.c_str()))
        {
            continue;
        }
        if (slash != std::string::npos || entry->is_directory)
        {
            add_folder_once(picker, first.c_str());
        }
        else if (picker->show_all || core == nullptr || libretro_core_info_accepts(core, rest.c_str()))
        {
            add_file(picker, rest.c_str());
        }
    }
}

static void list_folder(FilePicker* picker, const CoreCatalog* catalog)
{
    picker->folder_count = 0;
    picker->file_count = 0;
    picker->selected_file = -1;
    picker->listing_stale = false;

    const LibretroCoreInfo* core = selected_core(picker, catalog);
    std::string archive;
    std::string folder;
    if (archive_split_path(picker->directory, &archive, &folder))
    {
        list_archive(picker, core, &archive, &folder);
        std::qsort(picker->folders, static_cast<size_t>(picker->folder_count), FILE_PICKER_NAME_MAX, compare_names);
        std::qsort(picker->files, static_cast<size_t>(picker->file_count), FILE_PICKER_NAME_MAX, compare_names);
        return;
    }

    int count = 0;
    char** names = SDL_GlobDirectory(picker->directory, "*", 0, &count);
    for (int i = 0; names != nullptr && i < count; i++)
    {
        const char* name = names[i];
        if (name[0] == '.' || std::strlen(name) >= FILE_PICKER_NAME_MAX)
        {
            continue;   // hidden files and folders, and names we can't hold
        }
        char path[LIBRETRO_PATH_MAX] = {};
        SDL_PathInfo info = {};
        if (!join_path(picker->directory, name, path, sizeof(path)) || !SDL_GetPathInfo(path, &info))
        {
            continue;
        }
        bool archive_folder = info.type == SDL_PATHTYPE_FILE && archive_format(name) != ArchiveFormat::none &&
                              !content_archive_core_takes_whole(core, name);
        if (info.type == SDL_PATHTYPE_DIRECTORY || archive_folder)
        {
            add_folder(picker, name);
        }
        else if (info.type == SDL_PATHTYPE_FILE)
        {
            // Only what the chosen core says it loads (its own extension list).
            if (picker->show_all || core == nullptr || libretro_core_info_accepts(core, name))
            {
                add_file(picker, name);
            }
        }
    }
    if (names != nullptr)
    {
        SDL_free(names);
    }
    std::qsort(picker->folders, static_cast<size_t>(picker->folder_count), FILE_PICKER_NAME_MAX, compare_names);
    std::qsort(picker->files, static_cast<size_t>(picker->file_count), FILE_PICKER_NAME_MAX, compare_names);
}

// ---------------------------------------------------------------------------
// Public functions
// ---------------------------------------------------------------------------

void file_picker_open(FilePicker* picker, const CoreCatalog* catalog, const char* directory, const char* core_path)
{
    picker->open = true;
    picker->listing_stale = true;

    picker->archive_path.clear();   // the archive may have changed since: read it again
    picker->archive_entries.clear();

    // A folder, or a folder inside an archive that still exists.
    SDL_PathInfo info = {};
    std::string archive;
    std::string folder;
    bool in_archive = directory != nullptr && archive_split_path(directory, &archive, &folder);
    bool usable = in_archive ? SDL_GetPathInfo(archive.c_str(), &info) && info.type == SDL_PATHTYPE_FILE
                             : directory != nullptr && directory[0] != '\0' && SDL_GetPathInfo(directory, &info) &&
                                   info.type == SDL_PATHTYPE_DIRECTORY;
    if (usable)
    {
        std::snprintf(picker->directory, sizeof(picker->directory), "%s", directory);
    }
    else
    {
        const char* home = SDL_GetUserFolder(SDL_FOLDER_HOME);
        std::snprintf(picker->directory, sizeof(picker->directory), "%s", home != nullptr ? home : "/");
    }
    host_fs_forward_slashes(picker->directory);   // so the files picked from it don't mix separators

    // The given core if it's in the catalog, else the first usable one that
    // needs content (the catalog lists usable cores first).
    picker->core_index = -1;
    for (int32_t i = 0; i < catalog->count; i++)
    {
        char path[LIBRETRO_PATH_MAX] = {};
        if (core_path != nullptr && core_catalog_path(catalog, &catalog->entries[i], path, sizeof(path)) &&
            std::strcmp(path, core_path) == 0 && catalog->entries[i].info.usable)
        {
            picker->core_index = i;
            return;
        }
    }
    for (int32_t i = 0; i < catalog->count && picker->core_index < 0; i++)
    {
        if (catalog->entries[i].info.usable)
        {
            picker->core_index = i;
        }
    }
}

// Fills `choice` with the selected core and `file_name` (nullptr = no content).
static bool choose(FilePicker* picker, const CoreCatalog* catalog, const char* file_name, FilePickerChoice* choice)
{
    *choice = FilePickerChoice{};
    if (picker->core_index < 0 ||
        !core_catalog_path(catalog, &catalog->entries[picker->core_index], choice->core_path, sizeof(choice->core_path)))
    {
        return false;
    }
    if (file_name != nullptr && !join_path(picker->directory, file_name, choice->content_path, sizeof(choice->content_path)))
    {
        return false;
    }
    picker->open = false;
    return true;
}

bool file_picker_draw(FilePicker* picker, const CoreCatalog* catalog, ImFont* pixel_font, int32_t scale,
                      const Localization* strings, FilePickerChoice* choice)
{
    if (!picker->open)
    {
        return false;
    }
    if (picker->listing_stale)
    {
        list_folder(picker, catalog);
    }

    bool picked = false;
    float block = static_cast<float>(scale);
    const ImGuiIO& io = ImGui::GetIO();

    retro_ui_push_theme(pixel_font, scale);
    // 400 x 250 retro pixels, but never bigger than the window.
    ImVec2 size(400.0f * block, 250.0f * block);
    size.x = size.x < io.DisplaySize.x - 8.0f * block ? size.x : io.DisplaySize.x - 8.0f * block;
    size.y = size.y < io.DisplaySize.y - 24.0f * block ? size.y : io.DisplaySize.y - 24.0f * block;
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    bool keep_open = true;
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    char title[200] = {};
    std::snprintf(title, sizeof(title), "%s###load_game", piccolo_text(strings, Str::picker_title));
    if (ImGui::Begin(title, nullptr, flags))
    {
        retro_ui_begin_window(scale);
        if (retro_ui_close_button())
        {
            keep_open = false;
        }

        // Core first: it decides which files are shown.
        const LibretroCoreInfo* core = selected_core(picker, catalog);
        char preview[LIBRETRO_INFO_TEXT_MAX * 2 + 8] = {};
        std::snprintf(preview, sizeof(preview), "%s %s", core != nullptr ? core->name : piccolo_text(strings, Str::picker_no_cores),
                      core != nullptr ? core->version : "");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(piccolo_text(strings, Str::picker_core));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (retro_ui_begin_combo("##core", preview))
        {
            for (int32_t i = 0; i < catalog->count; i++)
            {
                const CoreCatalogEntry* entry = &catalog->entries[i];
                if (!entry->info.usable)
                {
                    continue;
                }
                char label[LIBRETRO_INFO_TEXT_MAX * 2 + 280] = {};
                std::snprintf(label, sizeof(label), "%s %s##%s", entry->info.name, entry->info.version, entry->file_name);
                if (ImGui::Selectable(label, i == picker->core_index))
                {
                    picker->core_index = i;
                    picker->listing_stale = true;
                }
            }
            ImGui::EndCombo();
        }

        // Files (left) and folders (right), leaving room for two rows below.
        float row = ImGui::GetFrameHeightWithSpacing();
        float list_height = ImGui::GetContentRegionAvail().y - row * 2.0f;
        float files_width = ImGui::GetContentRegionAvail().x * 0.62f;

        retro_ui_begin_list("files", files_width, list_height);
        for (int32_t i = 0; i < picker->file_count; i++)
        {
            ImGui::PushID(i);
            if (ImGui::Selectable(picker->files[i], i == picker->selected_file, ImGuiSelectableFlags_AllowDoubleClick))
            {
                picker->selected_file = i;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    picked = choose(picker, catalog, picker->files[i], choice);
                }
            }
            ImGui::PopID();
        }
        if (picker->file_count == 0)
        {
            ImGui::TextDisabled("%s", piccolo_text(strings, Str::picker_nothing_here));
        }
        retro_ui_end_list();

        ImGui::SameLine();
        retro_ui_begin_list("folders", 0.0f, list_height);
        const char* open_folder = nullptr;
        if (ImGui::Selectable(".."))
        {
            go_to_parent(picker->directory);
            picker->listing_stale = true;
        }
        for (int32_t i = 0; i < picker->folder_count; i++)
        {
            ImGui::PushID(i);
            if (ImGui::Selectable(picker->folders[i]))
            {
                open_folder = picker->folders[i];
            }
            ImGui::PopID();
        }
        retro_ui_end_list();
        if (open_folder != nullptr)
        {
            // An archive in a real folder opens as its top folder ("a.7z#").
            std::string archive;
            std::string folder;
            bool into_archive = !archive_split_path(picker->directory, &archive, &folder) &&
                                archive_format(open_folder) != ArchiveFormat::none;
            char next[LIBRETRO_PATH_MAX] = {};
            if (join_path(picker->directory, open_folder, next, sizeof(next)) &&
                (!into_archive || std::strlen(next) + 1 < sizeof(next)))
            {
                if (into_archive)
                {
                    std::strcat(next, "#");
                }
                std::snprintf(picker->directory, sizeof(picker->directory), "%s", next);
                picker->listing_stale = true;
            }
        }

        // Where we are.
        ImGui::TextDisabled("%s", picker->directory);

        // Bottom row.
        char show_all[200] = {};
        std::snprintf(show_all, sizeof(show_all), "%s###show_all", piccolo_text(strings, Str::picker_show_all));
        if (retro_ui_checkbox(show_all, &picker->show_all))
        {
            picker->listing_stale = true;
        }
        bool can_run_alone = core != nullptr && core->supports_no_game;
        const char* load = piccolo_text(strings, Str::picker_load);
        const char* alone = piccolo_text(strings, Str::picker_start_without_game);
        float load_width = ImGui::CalcTextSize(load).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float alone_width = ImGui::CalcTextSize(alone).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float right = load_width + (can_run_alone ? alone_width + ImGui::GetStyle().ItemSpacing.x : 0.0f);
        ImGui::SameLine(ImGui::GetContentRegionMax().x - right);
        ImGui::PushID("alone");
        bool start_alone = can_run_alone && retro_ui_button(alone);
        ImGui::PopID();
        if (start_alone)
        {
            picked = choose(picker, catalog, nullptr, choice);
        }
        if (can_run_alone)
        {
            ImGui::SameLine();
        }
        ImGui::BeginDisabled(picker->selected_file < 0);
        ImGui::PushID("load");
        bool load_clicked = retro_ui_button(load);
        ImGui::PopID();
        if (load_clicked && picker->selected_file >= 0)
        {
            picked = choose(picker, catalog, picker->files[picker->selected_file], choice);
        }
        ImGui::EndDisabled();

        retro_ui_end_window();
    }
    ImGui::End();
    retro_ui_pop_theme();

    if (!keep_open)
    {
        picker->open = false;
    }
    return picked;
}
