#include "bundled_cores.h"

#include <cstdint>
#include <cstring>
#include <string>

#include <dlfcn.h>

#include <SDL3/SDL.h>

#include "host_fs.h"

// The folder Android extracted our native libraries to: the one libmain.so
// was loaded from. dladdr on something inside libmain.so tells us its path.
static bool native_library_directory(std::string* directory)
{
    static const char marker = 0;
    Dl_info info = {};
    if (dladdr(&marker, &info) == 0 || info.dli_fname == nullptr)
    {
        return false;
    }
    const char* slash = std::strrchr(info.dli_fname, '/');
    if (slash == nullptr)
    {
        return false;
    }
    directory->assign(info.dli_fname, static_cast<size_t>(slash - info.dli_fname));
    return true;
}

void bundled_cores_install(const char* cores_directory)
{
    std::string library_directory;
    if (!native_library_directory(&library_directory))
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Bundled cores: can't find the native library folder");
        return;
    }

    int count = 0;
    char** names = SDL_GlobDirectory(library_directory.c_str(), "lib*_libretro.so", 0, &count);
    if (names == nullptr)
    {
        return;
    }
    for (int i = 0; i < count; i++)
    {
        // Read and written whole (host_fs_copy keeps existing files): a newer
        // APK must replace the old copy. host_fs_write_file writes atomically.
        std::string from = library_directory + "/" + names[i];
        std::string to = std::string(cores_directory) + "/" + (names[i] + 3);   // without "lib"
        uint8_t* data = nullptr;
        size_t size = 0;
        if (!host_fs_read_file(from.c_str(), &data, &size))
        {
            continue;
        }
        if (host_fs_write_file(to.c_str(), data, size))
        {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Bundled core: %s", to.c_str());
        }
        delete[] data;
    }
    SDL_free(names);
}
