#pragma once

// piccolo's "load game" window, in the retro style (retro_ui theme):
//
//   +- LOAD GAME ------------------------------------------+
//   | CORE  [ Snes9x 1.63                              v ] |
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
// Zip and 7z archives are folders (content_archive.h): opening one lists
// what's inside, and a file picked there is "a.7z#dir/Game.sfc", which the
// core session extracts. Cores that take archives whole (arcade cores,
// block_extract) see them as files instead.
// Esc or the window's close button closes the picker.

#include <cstdint>
#include <string>
#include <vector>

#include "archive.h"
#include "core_catalog.h"
#include "localization.h"

struct ImFont;

constexpr int32_t FILE_PICKER_MAX_ENTRIES = 2048;
constexpr size_t  FILE_PICKER_NAME_MAX    = 256;

struct FilePicker
{
    bool open = false;
    char directory[LIBRETRO_PATH_MAX] = {};

    int32_t core_index = -1;   // index into the catalog; -1 = none chosen yet
    bool    show_all   = false;

    // Current folder's contents, sorted (case-insensitive). Re-read when the
    // folder, the core or "show all" changes.
    char    folders[FILE_PICKER_MAX_ENTRIES][FILE_PICKER_NAME_MAX] = {};
    int32_t folder_count = 0;
    char    files[FILE_PICKER_MAX_ENTRIES][FILE_PICKER_NAME_MAX] = {};
    int32_t file_count = 0;
    int32_t selected_file = -1;
    bool    listing_stale = true;

    // Inside an archive (`directory` is "a.7z#" or "a.7z#dir"): its listing,
    // read once per archive rather than on every folder change.
    std::string               archive_path;
    std::vector<ArchiveEntry> archive_entries;
};

// What the user chose.
struct FilePickerChoice
{
    char core_path[LIBRETRO_PATH_MAX]    = {};
    char content_path[LIBRETRO_PATH_MAX] = {};   // empty = start without content
};

// Opens the window in `directory` with `core_path` preselected (either may be
// "" or not exist: then the home folder / the first usable core).
void file_picker_open(FilePicker* picker, const CoreCatalog* catalog, const char* directory, const char* core_path);

// Draws the window if open. Returns true on the frame the user picked
// something; the picker closes itself then.
bool file_picker_draw(FilePicker* picker, const CoreCatalog* catalog, ImFont* pixel_font, int32_t scale,
                      const Localization* strings, FilePickerChoice* choice);
