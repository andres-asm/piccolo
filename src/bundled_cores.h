#pragma once

// Cores that come inside piccolo's package. Android only for now: the APK
// carries them as lib/<abi>/lib<name>_libretro.so (Android only extracts
// files named lib*.so), next to libmain.so. At start they are copied into
// piccolo's cores folder as <name>_libretro.so, replacing older copies, so the
// core catalog lists them like any other core. Elsewhere this does nothing
// (bundled_cores_none.cpp). The core installer will add downloaded cores to
// the same folder.
void bundled_cores_install(const char* cores_directory);
