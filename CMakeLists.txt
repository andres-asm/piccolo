# piccolo: basic libretro frontend built on host-libretro.
# Plays games through frontend_libretro's core session, with a retro-style UI
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
    src/image_window.cpp
    src/piccolo_window.cpp
    src/piccolo_text.cpp
    src/recent_games.cpp
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

target_link_libraries(piccolo PRIVATE frontend_libretro piccolo_font)

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
