#include "bundled_files.h"

#include <SDL3/SDL.h>

// Desktop: the shipped files are already next to the program.

void bundled_files_install_cores(const char* cores_directory)
{
    (void)cores_directory;
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
