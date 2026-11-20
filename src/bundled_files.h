#pragma once

#include <string>

// Files that come inside piccolo's package, found where each platform puts them.
//
// Desktop (bundled_files.cpp): next to the program (SDL_GetBasePath); shipped
// cores (Test libretro) in its cores/ folder, copied into piccolo's cores
// folder when that's somewhere else (Flatpak).
//
// Android (bundled_files_android.cpp): the APK carries cores as
// lib/<abi>/lib<name>_libretro.so (Android only extracts files named lib*.so),
// next to libmain.so; layouts/, themes/ and shaders/ are APK assets, which SDL
// can list and read but host_fs (no SDL in hosts) and librashader can't. They
// are copied out at start.

// Copies the cores shipped in the package into piccolo's cores folder as
// <name>_libretro.so, replacing older copies, so the core catalog lists them
// like any other core. Call before the catalog first looks. Unchanged files
// aren't rewritten on desktop.
void bundled_files_install_cores(const char* cores_directory);

// The folder that holds the shipped layouts/, themes/ and shaders/, ending with a
// separator. Desktop: the program's folder. Android: a copy of the APK's
// assets in the app's private storage, refreshed when the APK changed (the
// activity's PICCOLO_APK_STAMP). Empty if there's none.
std::string bundled_files_data_folder();

// The name of piccolo's folder (app_paths: ~/.local/share/<name>,
// /sdcard/<name>). "piccolo", except Android debug builds: "piccolo-debug",
// told by the Java side (PICCOLO_FOLDER_NAME), because the debug package's
// cores live in its own private storage and a shared folder would list paths
// the other package can't open (core cache, RECENT).
const char* bundled_files_folder_name();
