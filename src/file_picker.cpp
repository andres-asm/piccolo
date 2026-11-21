#include "file_picker.h"

#include <algorithm>   // std::sort (the names)
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <SDL3/SDL.h>

#include "content_archive.h"
#include "host_fs.h"
#include "imgui.h"

#include "app_paths.h"

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

// Names in order, case-insensitive (std::sort's comparison).
static bool name_before(const std::string& a, const std::string& b)
{
    return SDL_strcasecmp(a.c_str(), b.c_str()) < 0;
}

static void sort_names(std::vector<std::string>* names)
{
    std::sort(names->begin(), names->end(), name_before);
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
    if (std::strlen(name) < FILE_PICKER_NAME_MAX)
    {
        picker->folders.push_back(name);
    }
}

static void add_file(FilePicker* picker, const char* name)
{
    if (std::strlen(name) < FILE_PICKER_NAME_MAX)
    {
        picker->files.push_back(name);
    }
}

// Archives list folders once per file inside them ("d/a", "d/b"): added once.
static void add_folder_once(FilePicker* picker, const char* name)
{
    for (size_t i = 0; i < picker->folders.size(); i++)
    {
        if (picker->folders[i] == name)
        {
            return;
        }
    }
    add_folder(picker, name);
}


// The archive's listing, unless it's the one already read.
static void read_archive(FilePicker* picker, const std::string* archive)
{
    if (picker->archive_path != *archive)
    {
        picker->archive_readable = archive_list(archive->c_str(), &picker->archive_entries);   // failure: logged
        picker->archive_path = *archive;
    }
}


// The folder `folder` ("" = the top) inside the archive: its subfolders and
// the files the core can load.
static void list_archive(FilePicker* picker, const LibretroCoreInfo* core, const std::string* archive,
                         const std::string* folder)
{
    read_archive(picker, archive);
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
        if (content_archive_hidden_name(first.c_str()))
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

// A shader preset: "*<extension>" (".slangp" / ".glpreset"), any case.
static bool is_shader_preset(const char* name, const char* extension)
{
    size_t length = std::strlen(name);
    size_t extension_length = std::strlen(extension);
    return length > extension_length && SDL_strcasecmp(name + length - extension_length, extension) == 0;
}

// Whether a file goes in the list.
static bool listed_file(const FilePicker* picker, const LibretroCoreInfo* core, const char* name)
{
    if (picker->mode == FilePickerMode::folder)
    {
        return false;   // folders only
    }
    if (picker->mode == FilePickerMode::shader_preset)
    {
        return is_shader_preset(name, picker->shader_extension);
    }
    // Only what the chosen core says it loads (its own extension list), and
    // every archive: what's inside is only known later.
    return picker->show_all || core == nullptr || libretro_core_info_accepts(core, name) ||
           archive_format(name) != ArchiveFormat::none;
}

static void list_folder(FilePicker* picker, const CoreCatalog* catalog)
{
    picker->folders.clear();
    picker->files.clear();
    picker->selected_file = -1;
    picker->listing_stale = false;

    const LibretroCoreInfo* core = selected_core(picker, catalog);
    std::string archive;
    std::string folder;
    if (archive_split_path(picker->directory, &archive, &folder))
    {
        list_archive(picker, core, &archive, &folder);
        sort_names(&picker->folders);
        sort_names(&picker->files);
        return;
    }

    // One listing with the types in it (host_fs_list_entries): no question
    // per file, which on a network share (a NAS) is a round trip each.
    std::vector<HostFsEntry> entries;
    host_fs_list_entries(picker->directory, &entries);   // (a failure is logged: an empty list)
    for (size_t i = 0; i < entries.size(); i++)
    {
        const char* name = entries[i].name.c_str();
        if (name[0] == '.' || entries[i].name.size() >= FILE_PICKER_NAME_MAX)
        {
            continue;   // hidden files and folders, and names we can't hold
        }
        if (entries[i].is_directory)
        {
            add_folder(picker, name);
        }
        else if (listed_file(picker, core, name))
        {
            add_file(picker, name);
        }
    }
    sort_names(&picker->folders);
    sort_names(&picker->files);
}

// ---------------------------------------------------------------------------
// Public functions
// ---------------------------------------------------------------------------

static bool is_directory(const char* path)
{
    SDL_PathInfo info = {};
    return path[0] != '\0' && SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

static void go_home(FilePicker* picker)
{
    const char* home = app_paths_home_folder();   // /sdcard on Android
    std::snprintf(picker->directory, sizeof(picker->directory), "%s", home != nullptr ? home : "/");
    host_fs_forward_slashes(picker->directory);
}

// Is `directory` `folder` or inside it ("/roms" holds "/roms/snes" and
// "/roms/a.7z#x", not "/roms2")? Forward slashes on both.
static bool folder_holds(const std::string* folder, const char* directory)
{
    size_t length = folder->size();
    while (length > 1 && (*folder)[length - 1] == '/')
    {
        length--;
    }
    if (std::strncmp(directory, folder->c_str(), length) != 0)
    {
        return false;
    }
    char next = directory[length];
    return next == '\0' || next == '/' || (length > 0 && (*folder)[length - 1] == '/');
}

// FOLDER's text: the game folder we're in (the longest, if they nest),
// else HOME when we're right there, else where we are.
static const char* folder_preview(const FilePicker* picker, const Localization* strings)
{
    int32_t best = -1;
    for (size_t i = 0; i < picker->game_folders.size(); i++)
    {
        if (folder_holds(&picker->game_folders[i], picker->directory) &&
            (best < 0 || picker->game_folders[i].size() > picker->game_folders[static_cast<size_t>(best)].size()))
        {
            best = static_cast<int32_t>(i);
        }
    }
    if (best >= 0)
    {
        return picker->game_folders[static_cast<size_t>(best)].c_str();
    }
    const char* home = app_paths_home_folder();   // /sdcard on Android
    if (home != nullptr)
    {
        char home_path[LIBRETRO_PATH_MAX] = {};
        std::snprintf(home_path, sizeof(home_path), "%s", home);
        host_fs_forward_slashes(home_path);
        std::string home_folder = home_path;
        if (folder_holds(&home_folder, picker->directory) && home_folder.size() + 1 >= std::strlen(picker->directory))
        {
            return piccolo_text(strings, Str::picker_home);   // the home folder itself (with or without its trailing '/')
        }
    }
    return picker->directory;
}

// The first game folder that exists, else the home folder.
static void go_to_first_game_folder(FilePicker* picker)
{
    go_home(picker);
    for (size_t i = 0; i < picker->game_folders.size(); i++)
    {
        if (is_directory(picker->game_folders[i].c_str()))
        {
            std::snprintf(picker->directory, sizeof(picker->directory), "%s", picker->game_folders[i].c_str());
            return;
        }
    }
}

void file_picker_open_folder(FilePicker* picker, const char* directory, const std::vector<std::string>* game_folders)
{
    picker->open = true;
    picker->mode = FilePickerMode::folder;
    picker->game_folders = *game_folders;
    picker->listing_stale = true;
    picker->selected_file = -1;
    picker->archive_path.clear();
    picker->archive_entries.clear();
    if (directory != nullptr && is_directory(directory))
    {
        std::snprintf(picker->directory, sizeof(picker->directory), "%s", directory);
        host_fs_forward_slashes(picker->directory);
    }
    else
    {
        go_to_first_game_folder(picker);
    }
}

void file_picker_open_shader(FilePicker* picker, const char* directory, const char* extension)
{
    std::snprintf(picker->shader_extension, sizeof(picker->shader_extension), "%s", extension);
    picker->open = true;
    picker->mode = FilePickerMode::shader_preset;
    picker->core_index = -1;
    picker->show_all = false;
    picker->game_folders.clear();
    picker->listing_stale = true;
    picker->selected_file = -1;
    picker->archive_path.clear();
    picker->archive_entries.clear();
    if (directory != nullptr && is_directory(directory))
    {
        std::snprintf(picker->directory, sizeof(picker->directory), "%s", directory);
        host_fs_forward_slashes(picker->directory);   // so the presets picked from it don't mix separators
    }
    else
    {
        go_home(picker);
    }
}

void file_picker_open(FilePicker* picker, const CoreCatalog* catalog, const char* directory, const char* core_path,
                      const std::vector<std::string>* game_folders)
{
    picker->open = true;
    picker->mode = FilePickerMode::game;
    picker->game_folders = *game_folders;
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
        host_fs_forward_slashes(picker->directory);   // so the files picked from it don't mix separators
    }
    else
    {
        go_to_first_game_folder(picker);
    }

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

// The selected file, if it's an archive in a real folder (archives inside
// archives are plain files: they aren't browsed). Only in LOAD GAME.
static bool selected_archive(const FilePicker* picker)
{
    std::string archive;
    std::string folder;
    return picker->mode == FilePickerMode::game && picker->selected_file >= 0 &&
           !archive_split_path(picker->directory, &archive, &folder) &&
           archive_format(picker->files[static_cast<size_t>(picker->selected_file)].c_str()) != ArchiveFormat::none;
}

// What's in the selected archive for the chosen core (see file_picker.h).
static void peek_selected(FilePicker* picker, const CoreCatalog* catalog)
{
    picker->peek_loadable = 0;
    picker->peek_single.clear();
    if (!selected_archive(picker))
    {
        return;
    }
    char path[LIBRETRO_PATH_MAX] = {};
    if (!join_path(picker->directory, picker->files[static_cast<size_t>(picker->selected_file)].c_str(), path, sizeof(path)))
    {
        return;
    }
    std::string archive = path;
    read_archive(picker, &archive);
    picker->peek_loadable = content_archive_loadable(selected_core(picker, catalog), &picker->archive_entries,
                                                     &picker->peek_single);
}

// Opens the selected archive as a folder ("a.7z#").
static void browse_selected(FilePicker* picker)
{
    char next[LIBRETRO_PATH_MAX] = {};
    if (join_path(picker->directory, picker->files[static_cast<size_t>(picker->selected_file)].c_str(), next, sizeof(next)) &&
        std::strlen(next) + 1 < sizeof(next))
    {
        std::strcat(next, "#");
        std::snprintf(picker->directory, sizeof(picker->directory), "%s", next);
        picker->listing_stale = true;
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

// A shader preset (`file_name`), or NONE (nullptr).
static bool choose_shader(FilePicker* picker, const char* file_name, FilePickerChoice* choice)
{
    *choice = FilePickerChoice{};
    if (file_name != nullptr && !join_path(picker->directory, file_name, choice->content_path, sizeof(choice->content_path)))
    {
        return false;
    }
    picker->open = false;
    return true;
}

// LOAD (or a double click) on the selected file: an archive follows the
// rules in file_picker.h. True if something was picked.
static bool load_selected(FilePicker* picker, const CoreCatalog* catalog, FilePickerChoice* choice)
{
    const char* name = picker->files[static_cast<size_t>(picker->selected_file)].c_str();
    if (picker->mode == FilePickerMode::shader_preset)
    {
        return choose_shader(picker, name, choice);
    }
    if (!selected_archive(picker) || content_archive_core_takes_whole(selected_core(picker, catalog), name))
    {
        return choose(picker, catalog, name, choice);
    }
    if (picker->peek_loadable == 1)
    {
        std::string inner = std::string(name) + "#" + picker->peek_single;
        return choose(picker, catalog, inner.c_str(), choice);
    }
    browse_selected(picker);
    return false;
}

// CHOOSE FOLDER's bottom row: USE THIS FOLDER (the folder shown) at the right.
static bool draw_folder_buttons(FilePicker* picker, const Localization* strings, FilePickerChoice* choice)
{
    bool picked = false;
    const char* use = piccolo_text(strings, Str::picker_use_folder);
    float use_width = ImGui::CalcTextSize(use).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - use_width);
    ImGui::PushID("use_folder");
    if (retro_ui_button(use))
    {
        *choice = FilePickerChoice{};
        std::snprintf(choice->content_path, sizeof(choice->content_path), "%s", picker->directory);
        picker->open = false;
        picked = true;
    }
    ImGui::PopID();
    return picked;
}

// The shader picker's bottom row: NONE and USE at the right.
static bool draw_shader_buttons(FilePicker* picker, const Localization* strings, FilePickerChoice* choice)
{
    bool picked = false;
    const char* none = piccolo_text(strings, Str::shader_preset_none);
    const char* use = piccolo_text(strings, Str::shader_picker_use);
    float padding = ImGui::GetStyle().FramePadding.x * 2.0f;
    float right = ImGui::CalcTextSize(none).x + ImGui::CalcTextSize(use).x + padding * 2.0f + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - right);
    ImGui::PushID("none");
    bool none_clicked = retro_ui_button(none);
    ImGui::PopID();
    if (none_clicked)
    {
        picked = choose_shader(picker, nullptr, choice);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(picker->selected_file < 0);
    ImGui::PushID("use");
    bool use_clicked = retro_ui_button(use);
    ImGui::PopID();
    if (use_clicked && picker->selected_file >= 0)
    {
        picked = choose_shader(picker, picker->files[static_cast<size_t>(picker->selected_file)].c_str(), choice);
    }
    ImGui::EndDisabled();
    return picked;
}

// LOAD GAME's bottom row: SHOW ALL FILES, then START WITHOUT GAME, BROWSE and
// LOAD at the right.
static bool draw_game_buttons(FilePicker* picker, const CoreCatalog* catalog, const LibretroCoreInfo* core,
                              const Localization* strings, FilePickerChoice* choice)
{
    bool picked = false;
    char show_all[200] = {};
    std::snprintf(show_all, sizeof(show_all), "%s###show_all", piccolo_text(strings, Str::picker_show_all));
    if (retro_ui_checkbox(show_all, &picker->show_all))
    {
        picker->listing_stale = true;
    }
    bool can_run_alone = core != nullptr && core->supports_no_game;
    const char* load = piccolo_text(strings, Str::picker_load);
    const char* browse = piccolo_text(strings, Str::picker_browse);
    const char* alone = piccolo_text(strings, Str::picker_start_without_game);
    float padding = ImGui::GetStyle().FramePadding.x * 2.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float load_width = ImGui::CalcTextSize(load).x + padding;
    float browse_width = ImGui::CalcTextSize(browse).x + padding;
    float alone_width = ImGui::CalcTextSize(alone).x + padding;
    float right = load_width + browse_width + spacing + (can_run_alone ? alone_width + spacing : 0.0f);
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
    // BROWSE: the selected archive as a folder, whatever the core.
    ImGui::BeginDisabled(!selected_archive(picker));
    ImGui::PushID("browse");
    bool browse_clicked = retro_ui_button(browse);
    ImGui::PopID();
    ImGui::EndDisabled();
    if (browse_clicked && selected_archive(picker))
    {
        browse_selected(picker);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(picker->selected_file < 0);
    ImGui::PushID("load");
    bool load_clicked = retro_ui_button(load);
    ImGui::PopID();
    if (load_clicked && picker->selected_file >= 0)
    {
        picked = load_selected(picker, catalog, choice);
    }
    ImGui::EndDisabled();
    return picked;
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
    bool choosing_folder = picker->mode == FilePickerMode::folder;
    bool shader = picker->mode == FilePickerMode::shader_preset;
    if (choosing_folder)
    {
        std::snprintf(title, sizeof(title), "%s###choose_folder", piccolo_text(strings, Str::picker_folder_title));
    }
    else if (shader)
    {
        std::snprintf(title, sizeof(title), "%s###shader_preset", piccolo_text(strings, Str::shader_picker_title));
    }
    else
    {
        std::snprintf(title, sizeof(title), "%s###load_game", piccolo_text(strings, Str::picker_title));
    }
    if (ImGui::Begin(title, nullptr, flags))
    {
        retro_ui_begin_window(scale);
        if (retro_ui_close_button())
        {
            keep_open = false;
        }

        // Core first: it decides which files are shown. Neither the core nor
        // the game folders matter when picking a shader preset.
        const LibretroCoreInfo* core = selected_core(picker, catalog);
        bool core_row = !choosing_folder && !shader;
        char preview[LIBRETRO_INFO_TEXT_MAX * 2 + 8] = {};
        std::snprintf(preview, sizeof(preview), "%s %s", core != nullptr ? core->name : piccolo_text(strings, Str::picker_no_cores),
                      core != nullptr ? core->version : "");
        const char* core_label = piccolo_text(strings, Str::picker_core);
        const char* folder_label = piccolo_text(strings, Str::picker_folder);
        float label_width = core_row ? ImGui::CalcTextSize(core_label).x : 0.0f;
        float folder_label_width = ImGui::CalcTextSize(folder_label).x;
        label_width = (label_width > folder_label_width ? label_width : folder_label_width) + ImGui::GetStyle().ItemSpacing.x;
        float combo_x = ImGui::GetCursorPosX() + label_width;
        if (core_row)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(core_label);
            ImGui::SameLine(combo_x);
            ImGui::SetNextItemWidth(-FLT_MIN);
        }
        if (core_row && retro_ui_begin_combo("##core", preview))
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

        // Where to start: a game folder or HOME.
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(folder_label);
        ImGui::SameLine(combo_x);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (retro_ui_begin_combo("##folder", folder_preview(picker, strings)))
        {
            if (ImGui::IsWindowAppearing())
            {
                app_paths_storage_volumes(&picker->volumes);   // read again every time the list opens
            }
            for (size_t i = 0; i < picker->game_folders.size(); i++)
            {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(picker->game_folders[i].c_str()))
                {
                    std::snprintf(picker->directory, sizeof(picker->directory), "%s", picker->game_folders[i].c_str());
                    picker->listing_stale = true;
                }
                ImGui::PopID();
            }
            ImGui::PushID("home");
            if (ImGui::Selectable(piccolo_text(strings, Str::picker_home)))
            {
                go_home(picker);
                picker->listing_stale = true;
            }
            ImGui::PopID();
            // SD cards and USB drives (Android), the drives (Windows), as they are while the list is open.
            for (size_t i = 0; i < picker->volumes.size(); i++)
            {
                ImGui::PushID(static_cast<int>(1000 + i));
                if (ImGui::Selectable(picker->volumes[i].c_str()))
                {
                    std::snprintf(picker->directory, sizeof(picker->directory), "%s", picker->volumes[i].c_str());
                    picker->listing_stale = true;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }

        // Files (left) and folders (right), leaving room for two rows below.
        float row = ImGui::GetFrameHeightWithSpacing();
        float list_height = ImGui::GetContentRegionAvail().y - row * 2.0f;
        float files_width = ImGui::GetContentRegionAvail().x * 0.62f;

        if (choosing_folder)
        {
            files_width = 0.0f;   // folders only, the whole width
        }
        else
        {
            retro_ui_begin_list("files", files_width, list_height);
        }
        int32_t file_count = static_cast<int32_t>(picker->files.size());
        if (!choosing_folder)
        {
            // Only the rows in view are drawn (thousands of files stay smooth;
            // the clipper keeps keyboard / gamepad navigation working).
            ImGuiListClipper clipper;
            clipper.Begin(file_count);
            while (clipper.Step())
            {
                for (int32_t i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                {
                    ImGui::PushID(i);
                    if (ImGui::Selectable(picker->files[static_cast<size_t>(i)].c_str(), i == picker->selected_file,
                                          ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        if (picker->selected_file != i)
                        {
                            picker->selected_file = i;
                            peek_selected(picker, catalog);
                        }
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        {
                            picked = load_selected(picker, catalog, choice);
                        }
                    }
                    ImGui::PopID();
                }
            }
        }
        if (!choosing_folder)
        {
            if (file_count == 0)
            {
                ImGui::TextDisabled("%s", piccolo_text(strings, Str::picker_nothing_here));
            }
            retro_ui_end_list();
            ImGui::SameLine();
        }
        retro_ui_begin_list("folders", 0.0f, list_height);
        const char* open_folder = nullptr;
        if (ImGui::Selectable(".."))
        {
            go_to_parent(picker->directory);
            picker->listing_stale = true;
        }
        ImGuiListClipper folder_clipper;
        folder_clipper.Begin(static_cast<int32_t>(picker->folders.size()));
        while (folder_clipper.Step())
        {
            for (int32_t i = folder_clipper.DisplayStart; i < folder_clipper.DisplayEnd; i++)
            {
                ImGui::PushID(i);
                if (ImGui::Selectable(picker->folders[static_cast<size_t>(i)].c_str()))
                {
                    open_folder = picker->folders[static_cast<size_t>(i)].c_str();
                }
                ImGui::PopID();
            }
        }
        retro_ui_end_list();
        if (open_folder != nullptr)
        {
            char next[LIBRETRO_PATH_MAX] = {};
            if (join_path(picker->directory, open_folder, next, sizeof(next)))
            {
                std::snprintf(picker->directory, sizeof(picker->directory), "%s", next);
                picker->listing_stale = true;
            }
        }

        // Where we are, or what LOAD does with the selected archive.
        if (selected_archive(picker))
        {
            const char* name = picker->files[static_cast<size_t>(picker->selected_file)].c_str();
            if (content_archive_core_takes_whole(core, name))
            {
                ImGui::TextDisabled("%s: %s", name, piccolo_text(strings, Str::picker_peek_whole));
            }
            else if (!picker->archive_readable)
            {
                ImGui::TextDisabled("%s: %s", name, piccolo_text(strings, Str::picker_peek_unreadable));
            }
            else if (picker->peek_loadable == 1)
            {
                ImGui::TextDisabled("%s: %s %s", name, piccolo_text(strings, Str::picker_peek_loads),
                                    picker->peek_single.c_str());
            }
            else if (picker->peek_loadable == 0)
            {
                ImGui::TextDisabled("%s: %s", name, piccolo_text(strings, Str::picker_peek_none));
            }
            else
            {
                ImGui::TextDisabled("%s: %s %d", name, piccolo_text(strings, Str::picker_peek_folder),
                                    static_cast<int>(picker->peek_loadable));
            }
        }
        else
        {
            ImGui::TextDisabled("%s", picker->directory);
        }

        // Bottom row. (A double click above may have picked already.)
        bool clicked = false;
        if (choosing_folder)
        {
            clicked = draw_folder_buttons(picker, strings, choice);
        }
        else if (shader)
        {
            clicked = draw_shader_buttons(picker, strings, choice);
        }
        else
        {
            clicked = draw_game_buttons(picker, catalog, core, strings, choice);
        }
        picked = picked || clicked;

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
