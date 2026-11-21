#include "storage.h"

#include <string>

#include <SDL3/SDL.h>
#include <emscripten/emscripten.h>

#include "app_paths.h"
#include "host_fs.h"

// The browser: piccolo's folder in memory, kept on OPFS by the page (see
// storage.h, web/page/piccolo-storage.js).

void storage_init(const char* folder_name)
{
    std::string folder;
    if (!app_paths_xdg_root(SDL_getenv("XDG_DATA_HOME"), SDL_getenv("HOME"), folder_name, &folder))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Storage: no home folder");
        return;
    }
    // The page made it and filled it from OPFS (XDG_DATA_HOME is its choice);
    // made here too for a page that doesn't.
    if (!host_fs_make_directories(folder.c_str()))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Storage: can't create %s", folder.c_str());
        return;
    }
    SDL_Log("Storage: %s in memory, kept on OPFS by the page", folder.c_str());
}

void storage_flush()
{
    // Async: it runs after this returns (the writes go on between frames).
    emscripten_run_script("if (self.piccolo_storage_sync) { piccolo_storage_sync(); }");
}
