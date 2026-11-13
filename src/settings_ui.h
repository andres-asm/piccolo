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

#include "settings.h"

// Which layer edits go to: SettingLayer::global, ::core or ::game.
struct SettingsUiContext
{
    Settings*    settings    = nullptr;
    SettingLayer scope       = SettingLayer::global;
    float        label_width = 0.0f;   // width of the label column
};

// One setting's row. `hint` is shown in an empty path field (e.g. the
// default folder); may be nullptr. Returns true if the value changed.
bool settings_ui_row(const SettingsUiContext* context, const char* key, const char* hint);

// Just the marker (for rows drawn by hand). Takes the space of one glyph
// even when there's no marker, so rows line up.
void settings_ui_marker(const Settings* settings, const char* key);

// Just the reset button at the right edge of the row, when the scope defines
// the value. Returns true if it reset it.
bool settings_ui_reset_button(const SettingsUiContext* context, const char* key);

// "GLOBAL", "THIS CORE", "THIS GAME".
const char* settings_ui_scope_name(SettingLayer scope);
