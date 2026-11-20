# piccolo: a libretro frontend, minimal in looks (inspired by classic emulator
# frontends of the 90s). Self-contained: every dependency is a submodule in
# deps/ (third-party ones, and multitap's libraries in deps/common and
# deps/libretro). Build:
#   git clone --recursive https://github.com/multitap-org/piccolo.git
#   cmake -S piccolo -B piccolo/build -G Ninja && cmake --build piccolo/build

cmake_minimum_required(VERSION 3.25)

# ccache as a compiler launcher when it's installed. Better than putting its
# wrapper folder first on PATH: on MSYS2 those wrappers are bash scripts, and
# ninja (a native Windows program) can't run them.
# Only when it actually runs (a half-updated MSYS2 can leave a ccache.exe that
# doesn't start, and then every compile fails).
find_program(MULTITAP_CCACHE ccache)
if(MULTITAP_CCACHE)
    execute_process(COMMAND "${MULTITAP_CCACHE}" --version
                    RESULT_VARIABLE _ccache_result OUTPUT_QUIET ERROR_QUIET)
    if(_ccache_result EQUAL 0)
        set(CMAKE_C_COMPILER_LAUNCHER   "${MULTITAP_CCACHE}" CACHE STRING "")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${MULTITAP_CCACHE}" CACHE STRING "")
    else()
        message(WARNING "ccache (${MULTITAP_CCACHE}) doesn't run; building without it")
    endif()
endif()

project(piccolo LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Options (the MULTITAP_ names are the project's: its libraries read them)
# ---------------------------------------------------------------------------
option(MULTITAP_SYSTEM_SDL3        "Use the system SDL3 instead of deps/SDL" OFF)
option(MULTITAP_WARNINGS_AS_ERRORS "Treat warnings in our own code as errors"    OFF)
option(MULTITAP_SANITIZE           "Build our own code with ASan + UBSan"         OFF)
option(MULTITAP_BUILD_TESTS        "Build tests and register CTest tests"         ON)

# Where builds are copied so they're easy to find and run: piccolo's own
# folder, the same one it uses for its data (frontends-common's app_paths.h).
# Linux: $XDG_DATA_HOME/piccolo (default ~/.local/share/piccolo). macOS:
# ~/Library/Application Support/piccolo. Windows: portable (its data goes next
# to the .exe), so any folder works; default %USERPROFILE%/piccolo.
# Only from normal builds: sanitizer builds need the ASan runtime (an ASan core
# can't be loaded by a normal frontend), so they keep everything in the build
# folder. Empty = don't copy.
if(MULTITAP_SANITIZE)
    set(_programs_base "")
elseif(WIN32)
    set(_programs_base "$ENV{USERPROFILE}")
elseif(APPLE)
    set(_programs_base "$ENV{HOME}/Library/Application Support")
elseif(DEFINED ENV{XDG_DATA_HOME} AND IS_ABSOLUTE "$ENV{XDG_DATA_HOME}")
    set(_programs_base "$ENV{XDG_DATA_HOME}")
else()
    set(_programs_base "$ENV{HOME}/.local/share")
endif()
set(_piccolo_dir_default "")
if(_programs_base)
    set(_piccolo_dir_default "${_programs_base}/piccolo")
endif()
set(MULTITAP_PICCOLO_DIR "${_piccolo_dir_default}" CACHE PATH "Copy piccolo here after building (empty = don't)")

# A git worktree (its .git is a file, not a folder; so is a submodule
# checkout) doesn't copy there unless asked: parallel work in worktrees would
# otherwise overwrite the piccolo the main checkout installed (the one being
# tried). Run ./build/piccolo from the worktree instead, or turn this on.
option(MULTITAP_COPY_FROM_WORKTREE "Copy to MULTITAP_PICCOLO_DIR from a git worktree too" OFF)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git" AND NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.git" AND
   NOT MULTITAP_COPY_FROM_WORKTREE)
    message(STATUS "piccolo: a git worktree, not copied to ${MULTITAP_PICCOLO_DIR} (MULTITAP_COPY_FROM_WORKTREE=ON to copy)")
    set(MULTITAP_PICCOLO_DIR "")   # hides the cache value for this configure only
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Submodule check
# ---------------------------------------------------------------------------
foreach(_dep imgui/imgui.cpp libretro-common/include/libretro.h libsamplerate/src/samplerate.c
             common/hosts-common/CMakeLists.txt common/frontends-common/CMakeLists.txt
             libretro/host-libretro/CMakeLists.txt libretro/frontend-libretro/CMakeLists.txt)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/${_dep}")
        message(FATAL_ERROR "deps/${_dep} is missing. Run: git submodule update --init --recursive")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Third-party dependencies (built with their own defaults, not our warnings)
# ---------------------------------------------------------------------------
find_package(Vulkan REQUIRED)

if(MULTITAP_SYSTEM_SDL3)
    find_package(SDL3 CONFIG REQUIRED)
else()
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/SDL/CMakeLists.txt")
        message(FATAL_ERROR "deps/SDL is empty. Run: git submodule update --init --recursive")
    endif()
    set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
    add_subdirectory(deps/SDL EXCLUDE_FROM_ALL SYSTEM)
endif()

# Dear ImGui core + SDL3 platform backend + Vulkan renderer backend.
# Sources come straight from the submodule; nothing is copied or patched.
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui")
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui SYSTEM PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3 Vulkan::Vulkan)

# libretro API header. Only include/libretro*.h is used; none of
# libretro-common's .c files are compiled (see CLAUDE.md).
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/libretro-common/include/libretro.h")
    message(FATAL_ERROR "deps/libretro-common is empty. Run: git submodule update --init --recursive")
endif()
add_library(libretro_api INTERFACE)
target_include_directories(libretro_api SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/deps/libretro-common/include")

if(MULTITAP_BUILD_TESTS)
    enable_testing()
endif()

# ---------------------------------------------------------------------------
# multitap's libraries (submodules). hosts-common first: it brings the
# project's CMake helpers (multitap_set_project_flags, ...).
# ---------------------------------------------------------------------------
set(MULTITAP_DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps")
add_subdirectory(deps/common/hosts-common)
# Test libretro (optional submodule, not cloned by default:
#   git submodule update --init --recursive --checkout deps/test-libretro
# ): into piccolo's cores folder, and host-libretro's tests run with it.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/test-libretro/CMakeLists.txt")
    add_subdirectory(deps/test-libretro)
    if(MULTITAP_PICCOLO_DIR)
        multitap_copy_after_build(test_libretro "${MULTITAP_PICCOLO_DIR}/cores")
    endif()
endif()
add_subdirectory(deps/libretro/host-libretro)
add_subdirectory(deps/common/frontends-common)
add_subdirectory(deps/libretro/frontend-libretro)

# ---------------------------------------------------------------------------
# piccolo
# ---------------------------------------------------------------------------
# Plays games through frontend-libretro's core session, with a retro-style UI
# (themed ImGui widgets with piccolo's pixel font).

# piccolo's own pixel font (5x5, drawn for piccolo; see src/pixel_font.h).
# A small library so the preview tool and, later, piccolo's UI share it.
add_library(piccolo_font STATIC
    src/pixel_font.cpp
)
target_include_directories(piccolo_font PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
multitap_set_project_flags(piccolo_font)

add_executable(piccolo
    src/main.cpp
    src/menus.cpp
    src/retro_ui.cpp
    src/pixel_font_imgui.cpp
    src/file_picker.cpp
    src/piccolo_settings.cpp
    src/settings_ui.cpp
    src/settings_window.cpp
    src/input_window.cpp
    src/net_window.cpp
    src/net_games.cpp
    src/net_remote.cpp
    src/map_window.cpp
    src/pad_picture.cpp
    src/image_window.cpp
    src/piccolo_window.cpp
    src/piccolo_text.cpp
    src/recent_games.cpp
    src/core_options_ui.cpp
    src/cores_window.cpp
    src/safe_start_window.cpp
    src/quick_menu.cpp
)

# UI text: src/strings.txt is the one English source. The build turns it into
# the string IDs (enum class Str), the English table and piccolo.pot (the
# translators' template), all in the build folder.
set(PICCOLO_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(
    OUTPUT "${PICCOLO_GENERATED}/piccolo_strings.h" "${PICCOLO_GENERATED}/piccolo_strings.cpp"
           "${PICCOLO_GENERATED}/piccolo.pot"
    COMMAND multitap_strings_generator "${CMAKE_CURRENT_SOURCE_DIR}/src/strings.txt" piccolo "${PICCOLO_GENERATED}"
    DEPENDS multitap_strings_generator "${CMAKE_CURRENT_SOURCE_DIR}/src/strings.txt"
    COMMENT "Generating piccolo's string IDs from strings.txt"
    VERBATIM)
target_sources(piccolo PRIVATE "${PICCOLO_GENERATED}/piccolo_strings.cpp")
target_include_directories(piccolo PRIVATE "${PICCOLO_GENERATED}")

# Translations (lang/<code>.po) next to the program, in the build folder and
# in piccolo's own folder.
add_custom_command(TARGET piccolo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/lang" "$<TARGET_FILE_DIR:piccolo>/lang"
    VERBATIM)
if(MULTITAP_PICCOLO_DIR)
    add_custom_target(copy_piccolo_lang ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/lang" "${MULTITAP_PICCOLO_DIR}/lang"
        DEPENDS piccolo
        COMMENT "Copying piccolo's translations to ${MULTITAP_PICCOLO_DIR}/lang"
        VERBATIM)
endif()

# Controller layouts (layouts/<id>.cfg) next to the program and in piccolo's
# folder, like the translations: anyone can add one (see input_layouts.h).
add_custom_command(TARGET piccolo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/layouts" "$<TARGET_FILE_DIR:piccolo>/layouts"
    VERBATIM)
if(MULTITAP_PICCOLO_DIR)
    add_custom_target(copy_piccolo_layouts ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/layouts" "${MULTITAP_PICCOLO_DIR}/layouts"
        DEPENDS piccolo
        COMMENT "Copying piccolo's controller layouts to ${MULTITAP_PICCOLO_DIR}/layouts"
        VERBATIM)
endif()

# Interface themes (themes/<name>.cfg) next to the program and in piccolo's
# folder, like the layouts: anyone can add one (see themes/example.cfg).
add_custom_command(TARGET piccolo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/themes" "$<TARGET_FILE_DIR:piccolo>/themes"
    VERBATIM)
if(MULTITAP_PICCOLO_DIR)
    add_custom_target(copy_piccolo_themes ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/themes" "${MULTITAP_PICCOLO_DIR}/themes"
        DEPENDS piccolo
        COMMENT "Copying piccolo's interface themes to ${MULTITAP_PICCOLO_DIR}/themes"
        VERBATIM)
endif()

target_link_libraries(piccolo PRIVATE frontend_libretro frontend_net piccolo_font)

# Windows: the .exe's icon, as a resource (assets/windows/piccolo.ico, made by
# tools/make_app_icon.py from the same PNGs as the window icon).
if(WIN32)
    enable_language(RC)
    set(PICCOLO_ICO "${CMAKE_CURRENT_SOURCE_DIR}/assets/windows/piccolo.ico")
    configure_file(src/piccolo.rc.in "${PICCOLO_GENERATED}/piccolo.rc" @ONLY)
    target_sources(piccolo PRIVATE "${PICCOLO_GENERATED}/piccolo.rc")
    set_source_files_properties("${PICCOLO_GENERATED}/piccolo.rc" PROPERTIES OBJECT_DEPENDS "${PICCOLO_ICO}")
endif()

# piccolo's version: piccolo_version.h,
# PICCOLO_VERSION. The branch suffix is piccolo's own branch.
set(PICCOLO_VERSION_BASE 0.3)
multitap_version(PICCOLO_VERSION ${PICCOLO_VERSION_BASE} "${CMAKE_CURRENT_SOURCE_DIR}")
multitap_write_version_header("${PICCOLO_GENERATED}" piccolo PICCOLO_VERSION "${PICCOLO_VERSION}")
message(STATUS "piccolo ${PICCOLO_VERSION}")

multitap_set_project_flags(piccolo)
multitap_copy_after_build(piccolo "${MULTITAP_PICCOLO_DIR}")

# Development tool: writes a PNG of every glyph + sample text, to judge the font by eye.
add_executable(pixel_font_preview tools/pixel_font_preview.cpp)
target_link_libraries(pixel_font_preview PRIVATE piccolo_font)
multitap_set_project_flags(pixel_font_preview)

if(MULTITAP_BUILD_TESTS)
    # Table well formed, no character defined twice, no two glyphs identical.
    add_test(NAME piccolo_pixel_font COMMAND pixel_font_preview --check)
    set_tests_properties(piccolo_pixel_font PROPERTIES PASS_REGULAR_EXPRESSION "table OK")

    # The UI text against the translations and the font (missing translations,
    # stale keys, characters the font can't draw). Unknown keys in a .po are
    # reported in the log too.
    add_executable(piccolo_strings_check tools/strings_check.cpp "${PICCOLO_GENERATED}/piccolo_strings.cpp")
    target_include_directories(piccolo_strings_check PRIVATE "${PICCOLO_GENERATED}")
    target_link_libraries(piccolo_strings_check PRIVATE frontend_common piccolo_font)
    multitap_set_project_flags(piccolo_strings_check)
    add_test(NAME piccolo_strings COMMAND piccolo_strings_check "${CMAKE_CURRENT_SOURCE_DIR}/lang" es)
    set_tests_properties(piccolo_strings PROPERTIES
        FAIL_REGULAR_EXPRESSION "some keys are unknown|ERROR: AddressSanitizer|ERROR: LeakSanitizer|runtime error:")

    add_executable(piccolo_recent_games_test tools/recent_games_test.cpp src/recent_games.cpp)
    target_include_directories(piccolo_recent_games_test PRIVATE src)
    target_link_libraries(piccolo_recent_games_test PRIVATE frontend_libretro)
    multitap_set_project_flags(piccolo_recent_games_test)
    add_test(NAME piccolo_recent_games
             COMMAND piccolo_recent_games_test "${CMAKE_CURRENT_BINARY_DIR}/recent_games_test")
    set_tests_properties(piccolo_recent_games PROPERTIES
        FAIL_REGULAR_EXPRESSION "ERROR: AddressSanitizer|ERROR: LeakSanitizer|runtime error:")
endif()
