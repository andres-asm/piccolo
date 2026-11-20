#include "storage.h"

#include <string>

#include <SDL3/SDL.h>
#include <emscripten/wasmfs.h>

#include "app_paths.h"
#include "host_fs.h"

// The browser: piccolo's folder on OPFS (see storage.h).

struct StorageMount
{
    std::string folder;
    bool        mounted = false;
};

// WasmFS refuses to create its OPFS backend on the browser's main thread (it
// would wait there for its own worker to start: a deadlock). So it's made on
// a thread of ours, which the main thread waits for. Both threads come from
// Emscripten's pool, started with the page (-sPTHREAD_POOL_SIZE), so the
// wait doesn't need the main thread to go back to the browser.
static int mount_thread(void* data)
{
    StorageMount* mount = static_cast<StorageMount*>(data);
    backend_t opfs = wasmfs_create_opfs_backend();
    if (opfs == nullptr)
    {
        return 0;
    }
    mount->mounted = wasmfs_create_directory(mount->folder.c_str(), 0777, opfs) == 0;
    return 0;
}

void storage_init(const char* folder_name)
{
    StorageMount mount = {};
    if (!app_paths_xdg_root(SDL_getenv("XDG_DATA_HOME"), SDL_getenv("HOME"), folder_name, &mount.folder))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Storage: no home folder, nothing will be kept");
        return;
    }
    // The folders above it, in memory (only the last one is OPFS).
    std::string parent = mount.folder.substr(0, mount.folder.rfind('/'));
    if (!host_fs_make_directories(parent.c_str()))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Storage: can't create %s", parent.c_str());
        return;
    }
    SDL_Thread* thread = SDL_CreateThread(mount_thread, "opfs mount", &mount);
    if (thread == nullptr)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Storage: no thread to mount OPFS: %s", SDL_GetError());
        return;
    }
    SDL_WaitThread(thread, nullptr);
    if (mount.mounted)
    {
        SDL_Log("Storage: %s on OPFS (kept in this browser)", mount.folder.c_str());
    }
    else
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Storage: OPFS unavailable, nothing will be kept after this tab closes");
    }
}
