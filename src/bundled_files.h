#pragma once

#include <string>

// Files that come inside piccolo's package, found where each platform puts them.
//
// Desktop: next to the program (SDL_GetBasePath); nothing to install
// (bundled_files.cpp).
//
// Android (bundled_files_android.cpp): the APK carries cores as
// lib/<abi>/lib<name>_libretro.so (Android only extracts files named lib*.so),
// next to libmain.so; layouts/ and themes/ are APK assets, which SDL can list
// and read but host_fs (no SDL in hosts) can't. Both are copied out at start.

// Copies the cores shipped in the package into piccolo's cores folder as
// <name>_libretro.so, replacing older copies, so the core catalog lists them
// like any other core. Call before the catalog first looks. Desktop: nothing.
void bundled_files_install_cores(const char* cores_directory);

// The folder that holds the shipped layouts/ and themes/, ending with a
// separator. Desktop: the program's folder. Android: a copy of the APK's
// assets in the app's private storage, refreshed at every start (a few small
// files). Empty if there's none.
std::string bundled_files_data_folder();
