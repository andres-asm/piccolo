#pragma once

// Widgets for editing settings (settings.h) in piccolo's retro style. Shared
// by the settings window and the crop window. Use inside the retro_ui theme.
//
// One row per setting:
//
//   M LABEL           [ widget                         ] [RESET]
//
//   M      override marker: "C" (set for this core) or "G" (set for this
//          game) in its own color, with a tooltip. Not color alone.
//   widget shows the value as it resolves at the scope being edited, and a
//          change goes to that scope.
//   RESET  shown when the scope being edited defines the value; removes it
//          from that scope (back to the layer below).
// Global-only settings are greyed out while a core or game scope is edited.

#include "localization.h"
#include "settings.h"

// Which layer edits go to: SettingLayer::global, ::core or ::game.
struct SettingsUiContext
{
    Settings*           settings    = nullptr;
    const Localization* strings     = nullptr;   // UI text (piccolo_text.h)
    SettingLayer scope       = SettingLayer::global;
    float        label_width = 0.0f;   // width of the label column
};

// One setting's row. `hint` is shown in an empty path field (e.g. the
// default folder); may be nullptr. Returns true if the value changed.
bool settings_ui_row(const SettingsUiContext* context, const char* key, const char* hint);

// A folder setting changed with CHOOSE FOLDER: the row of settings_ui_row,
// but the field is read-only and a click on it returns true (the caller
// opens the picker). `hint` as in settings_ui_row; RESET as usual.
bool settings_ui_folder_row(const SettingsUiContext* context, const char* key, const char* hint);

// The field of a folder row: `value` read-only (or `hint` when empty) and
// "..." after it, together as wide as settings_ui_row's widget. True when
// either was clicked (the caller opens CHOOSE FOLDER).
bool settings_ui_folder_field(const SettingsUiContext* context, const char* value, const char* hint);

// The reset button's width: for rows drawn by hand that line up with the
// others (a button in its column).
float settings_ui_reset_width(const SettingsUiContext* context);

// A text setting picked from a list (e.g. the audio devices connected now).
// "" shows `empty_label`; a stored value that isn't in the list is shown
// with `missing_label` after it, and kept until something else is picked.
// Returns true if the value changed.
bool settings_ui_text_choice_row(const SettingsUiContext* context, const char* key, const char* const* choices,
                                 uint32_t choice_count, const char* empty_label, const char* missing_label);

// Just the marker (for rows drawn by hand). Takes the space of one glyph
// even when there's no marker, so rows line up.
void settings_ui_marker(const SettingsUiContext* context, const char* key);

// Just the reset button at the right edge of the row, when the scope defines
// the value. Returns true if it reset it.
bool settings_ui_reset_button(const SettingsUiContext* context, const char* key);

// "GLOBAL", "THIS CORE", "THIS GAME" in the current language.
const char* settings_ui_scope_name(const Localization* strings, SettingLayer scope);

// The label column's width: the widest label of the rows that have one.
float settings_ui_label_width(const Settings* settings, const Localization* strings);

// The SCOPE selector: GLOBAL / THIS CORE (name) / THIS GAME (name), only the
// ones that exist now. With *choose_scope set (just opened) it picks the most
// specific layer already in use, so changes keep going there, and clears it.
void settings_ui_scope(Settings* settings, const Localization* strings, const char* core_name, const char* game_name,
                       SettingLayer* scope, bool* choose_scope);
