#include "bundled_files.h"

#include <cstdint>
#include <cstring>
#include <string>

#include <SDL3/SDL.h>

#include "host_fs.h"

// Desktop: the shipped files are next to the program. Cores shipped with it
// (Test libretro) are in <program folder>/cores. When that already is piccolo's
// cores folder (Windows portable, piccolo copied to ~/.local/share/piccolo)
// there's nothing to do; otherwise (Flatpak: /app/lib/piccolo/cores, read only)
// they're copied into piccolo's cores folder so the catalog lists them.

// True if the file at `path` already holds exactly `data`: then it isn't
// rewritten, which would change its time and make the catalog peek it again.
static bool file_has_contents(const char* path, const uint8_t* data, size_t size)
{
    uint8_t* existing = nullptr;
    size_t existing_size = 0;
    if (!host_fs_read_file(path, &existing, &existing_size))
    {
        return false;
    }
    bool same = existing_size == size && std::memcmp(existing, data, size) == 0;
    delete[] existing;
    return same;
}

void bundled_files_install_cores(const char* cores_directory)
{
    const char* program_folder = SDL_GetBasePath();   // ends with a separator; SDL owns it
    if (program_folder == nullptr)
    {
        return;
    }
    std::string bundled = std::string(program_folder) + "cores";
    std::string target = cores_directory;
    host_fs_forward_slashes(bundled.data());
    host_fs_forward_slashes(target.data());
    if (bundled == target)
    {
        return;
    }

    int count = 0;
    char** names = SDL_GlobDirectory(bundled.c_str(), "*_libretro.*", 0, &count);
    if (names == nullptr)
    {
        return;   // no bundled cores folder
    }
    for (int i = 0; i < count; i++)
    {
        std::string from = bundled + "/" + names[i];
        std::string to = target + "/" + names[i];
        uint8_t* data = nullptr;
        size_t size = 0;
        if (!host_fs_read_file(from.c_str(), &data, &size))
        {
            continue;
        }
        if (!file_has_contents(to.c_str(), data, size))
        {
            if (host_fs_write_file(to.c_str(), data, size))
            {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Bundled core: %s", to.c_str());
            }
        }
        delete[] data;
    }
    SDL_free(names);
}

std::string bundled_files_data_folder()
{
    const char* program_folder = SDL_GetBasePath();   // ends with a separator; SDL owns it
    return program_folder != nullptr ? program_folder : "";
}

const char* bundled_files_folder_name()
{
    return "piccolo";
}
