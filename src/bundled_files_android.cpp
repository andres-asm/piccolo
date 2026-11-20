#include "bundled_files.h"

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

void bundled_files_install_cores(const char* cores_directory)
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

// Copies every file under the APK asset folder `folder` (relative, e.g.
// "layouts") to destination/folder/, keeping subfolders. SDL lists and reads
// assets through relative paths (SDL_GlobDirectory, SDL_LoadFile).
static void copy_asset_folder(const char* folder, const std::string& destination)
{
    int count = 0;
    char** names = SDL_GlobDirectory(folder, nullptr, 0, &count);
    if (names == nullptr)
    {
        return;
    }
    for (int i = 0; i < count; i++)
    {
        std::string from = std::string(folder) + "/" + names[i];
        SDL_PathInfo info = {};
        if (!SDL_GetPathInfo(from.c_str(), &info) || info.type != SDL_PATHTYPE_FILE)
        {
            continue;   // folders are made as their files are written
        }
        std::string to = destination + from;
        size_t slash = to.rfind('/');
        if (slash != std::string::npos && !host_fs_make_directories(to.substr(0, slash).c_str()))
        {
            continue;
        }
        size_t size = 0;
        void* data = SDL_LoadFile(from.c_str(), &size);
        if (data == nullptr)
        {
            continue;
        }
        if (!host_fs_write_file(to.c_str(), data, size))
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Bundled files: can't write %s", to.c_str());
        }
        SDL_free(data);
    }
    SDL_free(names);
}

std::string bundled_files_data_folder()
{
    const char* internal = SDL_GetAndroidInternalStoragePath();   // SDL owns it
    if (internal == nullptr)
    {
        return "";
    }
    // Rewritten at every start, so a newer APK's files replace the old ones.
    // (A file a newer APK no longer ships stays behind; harmless for now.)
    std::string destination = std::string(internal) + "/bundled/";
    copy_asset_folder("layouts", destination);
    copy_asset_folder("themes", destination);
    return destination;
}

const char* bundled_files_folder_name()
{
    const char* name = SDL_getenv("PICCOLO_FOLDER_NAME");   // set by PiccoloActivity
    return (name != nullptr && name[0] != '\0') ? name : "piccolo";
}
