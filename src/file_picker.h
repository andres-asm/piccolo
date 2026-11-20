#pragma once

// piccolo's "load game" window, in the retro style (retro_ui theme):
//
//   +- LOAD GAME ------------------------------------------+
//   | CORE   [ Snes9x 1.63                             v ] |
//   | FOLDER [ /home/user/roms                         v ] |
//   | +- files -----------------------+ +- folders -----+ |
//   | | Super Mario World (USA).sfc   | | ..            | |
//   | | ...                           | | Nintendo - .. | |
//   | +-------------------------------+ +---------------+ |
//   | /home/user/Downloads/...                             |
//   | [x] SHOW ALL FILES        [START WITHOUT GAME] [LOAD] |
//   +-------------------------------------------------------+
//
// The core is picked first; the file list shows only what that core says it
// can load (its own extension list, from the core catalog). Double-click a
// file or press LOAD to start it; click a folder to open it.
//
// Zip and 7z archives are always in the file list (never filtered: what's
// inside isn't known until one is opened). Selecting one peeks at its index
// and the line under the lists says what LOAD will do (content_archive.h):
//   - the core takes archives whole (arcade cores, block_extract): the archive;
//   - else, exactly one file inside the core can load: that file
//     ("a.7z#dir/Game.sfc", extracted by the core session);
//   - else: opens it as a folder, like BROWSE, which always does that.
// FOLDER jumps to one of the game folders (SETTINGS > GENERAL) or HOME, and
// shows the one we're in. The picker isn't limited to them: they're only
// places to start (the network is limited to them, not the local user).
// Esc or the window's close button closes the picker.
//
// The same window chooses a folder (file_picker_open_folder: SETTINGS' game
// folders): CHOOSE FOLDER, no core, only folders (archives aren't folders
// here), and USE THIS FOLDER picks the folder shown.
//
// It also picks a shader preset (FilePickerMode::shader_preset, from
// SETTINGS > SHADER > CHOOSE): no core row, only .slangp files, no archives
// (presets are always plain files), and NONE / USE instead of the game
// buttons. It can go to any folder (e.g. a full slang-shaders checkout).

#include <cstdint>
#include <string>
#include <vector>

#include "archive.h"
#include "core_catalog.h"
#include "localization.h"

struct ImFont;

constexpr int32_t FILE_PICKER_MAX_ENTRIES = 2048;
constexpr size_t  FILE_PICKER_NAME_MAX    = 256;

enum class FilePickerMode
{
    game,            // LOAD GAME: a core and a file
    folder,          // CHOOSE FOLDER: a folder
    shader_preset,   // a .slangp for the SHADER setting
};

struct FilePicker
{
    bool open = false;
    FilePickerMode mode = FilePickerMode::game;
    char directory[LIBRETRO_PATH_MAX] = {};

    int32_t core_index = -1;   // index into the catalog; -1 = none chosen yet
    std::vector<std::string> game_folders;   // FOLDER's choices (copied at open), then HOME
    bool    show_all   = false;

    // Current folder's contents, sorted (case-insensitive). Re-read when the
    // folder, the core or "show all" changes.
    char    folders[FILE_PICKER_MAX_ENTRIES][FILE_PICKER_NAME_MAX] = {};
    int32_t folder_count = 0;
    char    files[FILE_PICKER_MAX_ENTRIES][FILE_PICKER_NAME_MAX] = {};
    int32_t file_count = 0;
    int32_t selected_file = -1;
    bool    listing_stale = true;

    // An archive's listing: the one browsed (`directory` is "a.7z#" or
    // "a.7z#dir") or peeked at, read once rather than on every change.
    std::string               archive_path;
    std::vector<ArchiveEntry> archive_entries;
    bool                      archive_readable = false;

    // The archive selected in the file list, from its listing.
    int32_t     peek_loadable = 0;   // files inside the chosen core can load
    std::string peek_single;         // the one file, when peek_loadable == 1
};

// What the user chose. CHOOSE FOLDER: the folder in content_path, no core.
// Shader preset: the preset in content_path, or empty for NONE; no core.
struct FilePickerChoice
{
    char core_path[LIBRETRO_PATH_MAX]    = {};
    char content_path[LIBRETRO_PATH_MAX] = {};   // empty = start without content
};

// Opens the window in `directory` with `core_path` preselected (either may be
// "" or not exist: then the first game folder that exists, else the home
// folder / the first usable core).
void file_picker_open(FilePicker* picker, const CoreCatalog* catalog, const char* directory, const char* core_path,
                      const std::vector<std::string>* game_folders);

// Opens CHOOSE FOLDER in `directory` (if it's a folder that exists; else
// the first game folder that does, else the home folder).
void file_picker_open_folder(FilePicker* picker, const char* directory, const std::vector<std::string>* game_folders);

// Opens the window to pick a shader preset, in `directory` (if it doesn't
// exist: the home folder).
void file_picker_open_shader(FilePicker* picker, const char* directory);

// Draws the window if open. Returns true on the frame the user picked
// something; the picker closes itself then.
bool file_picker_draw(FilePicker* picker, const CoreCatalog* catalog, ImFont* pixel_font, int32_t scale,
                      const Localization* strings, FilePickerChoice* choice);
