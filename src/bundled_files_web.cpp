#include "bundled_files.h"

#include <string>

#include "host_fs.h"
#include "libretro_linked_core.h"
#include "piccolo_linked_core.h"   // generated: PICCOLO_LINKED_CORE_FILE (CMakeLists.txt)

// The browser build (docs/wasm.md): one wasm per core, the core linked in.
// Its "file" is an empty marker in the cores folder, so the core catalog
// lists it like any other core; opening that name opens the linked core
// (libretro_linked_core.h). The data folders (layouts/, themes/, lang/,
// shaders/) are in the module's virtual filesystem at its root.

void bundled_files_install_cores(const char* cores_directory)
{
    libretro_linked_core_register(PICCOLO_LINKED_CORE_FILE);
    std::string marker = std::string(cores_directory) + "/" + PICCOLO_LINKED_CORE_FILE;
    if (!host_fs_exists(marker.c_str()))
    {
        host_fs_write_file(marker.c_str(), "", 0);
    }
}

std::string bundled_files_data_folder()
{
    return "/";
}

const char* bundled_files_folder_name()
{
    return "piccolo";
}
