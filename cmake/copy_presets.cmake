# Copies the curated shader presets (deps/piccolo-shaders) to DESTINATION,
# without the submodule's .git file. Run as:
#   cmake -DSOURCE=<deps/piccolo-shaders> -DDESTINATION=<.../shaders/presets> -P copy_presets.cmake
# Copies over what's there and removes nothing: piccolo's folder is also the
# user's, and they may keep their own files next to ours.

if(NOT SOURCE OR NOT DESTINATION)
    message(FATAL_ERROR "copy_presets.cmake needs SOURCE and DESTINATION")
endif()

file(MAKE_DIRECTORY "${DESTINATION}")
file(COPY "${SOURCE}/" DESTINATION "${DESTINATION}"
     PATTERN ".git" EXCLUDE)
