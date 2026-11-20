#pragma once

// piccolo's retro-style UI, built from themed Dear ImGui widgets with
// piccolo's pixel font (decided by Andrés, 2026-11-12, after comparing with a
// software-drawn version: ImGui gives navigation and widgets for free).
//
// "Retro pixels": everything is laid out in units of the 5x5 font's pixels,
// times `scale` window pixels, so the UI stays crisp at any whole-number scale.

#include <cstdint>

#include "localization.h"
#include "menus.h"

struct ImFont;

// The colors of the whole UI (0xAARRGGBB; alpha is always FF). Themes are
// palettes (decided by Andrés, 2026-11-14, renamed 2026-11-19): BLUE (navy, the
// default), PURPLE (atomic purple), ZSNES (a blue gradient bar with grey boxes
// behind the menu titles) and BLACK (flat, the dropdowns the same color as the
// bar); every color can
// also be set on its own in SETTINGS > INTERFACE.
struct RetroUiPalette
{
    uint32_t background;       // behind the game and the menu
    bool     shadows;          // drop shadows under windows, open menus and menu text
    uint32_t bar_face;         // the menu bar, top and bottom...
    uint32_t bar_center;       // ...and its middle (a vertical gradient; the same = flat)
    uint32_t bar_light;        // the bar's raised edge
    uint32_t bar_dark;
    bool     menu_boxes;       // a box behind each menu title on the bar
    uint32_t menu_box_face;
    uint32_t menu_box_light;   // also an open menu's raised edge
    uint32_t menu_box_dark;
    uint32_t menu_face;        // a dropdown menu's background
    uint32_t panel_face;       // windows
    uint32_t panel_light;
    uint32_t panel_dark;
    uint32_t field_face;       // fields, lists, inactive tabs
    uint32_t field_hover;
    uint32_t highlight;        // hovered / selected / pressed
    uint32_t text;
    uint32_t text_highlight;
    uint32_t text_disabled;
    uint32_t text_shadow;      // menu text's shadow; window shadows are it at 25% opacity
    uint32_t title_top;        // window title bars, top...
    uint32_t title_bottom;     // ...and bottom (a vertical gradient; the same = flat)
    uint32_t check;            // check marks
    uint32_t section;          // section titles in windows
    uint32_t button_face;      // buttons, check boxes and scroll bar grips, raised: top...
    uint32_t button_light;
    uint32_t button_dark;
    uint32_t button_text;
    uint32_t field_text;       // text in fields and combo boxes
    bool     theme_cursor;     // draw our own mouse cursor (the system's is hidden)
    uint32_t cursor;           // its color...
    uint32_t button_bottom;    // ...and bottom of buttons (a vertical gradient; the same = flat)
    uint32_t cursor_edge;      // ...and its top / right edge (the same = flat)
};

enum class RetroUiTheme
{
    blue,
    purple,
    zsnes,
    black,
    count,
};

// The built-in palette of a theme.
const RetroUiPalette* retro_ui_theme_palette(RetroUiTheme theme);

// The palette everything is drawn with from now on (copied).
void retro_ui_set_palette(const RetroUiPalette* palette);

// The scale for a window: the biggest whole block that leaves at least
// 640 x 360 retro pixels, at least 1 (3 on a 1920 x 1080 window).
int32_t retro_ui_scale_for_window(float window_width, float window_height);

// The menu bar at the top of the window. Call between ImGui::NewFrame() and
// rendering. Returns the item picked this frame (MenuAction::none if none).
// Labels in the language of `strings`.
//
// The first menu (a "hamburger" glyph) lists the recent games: `recent`
// holds their labels, most recent first. Picking one returns
// MenuAction::recent_game and its index in *recent_picked.
// `open_first_menu`: open that first menu now, with the keyboard / gamepad
// focus on its first item (to start navigating without a mouse).
MenuAction retro_ui_menu_bar(ImFont* pixel_font, int32_t scale, const Localization* strings,
                             const char* const* recent, int32_t recent_count, int32_t* recent_picked,
                             bool open_first_menu);

// One line of text at the bottom left of the window, behind other windows, in
// the pixel font with a drop shadow.
void retro_ui_status_line(ImFont* pixel_font, int32_t scale, const char* text);

// The retro look for any window and widget between push and pop: palette,
// pixel font at `scale`, square corners, spacing in retro pixels. Pair them.
void retro_ui_push_theme(ImFont* pixel_font, int32_t scale);
void retro_ui_pop_theme();

// Call right after ImGui::Begin() of a themed window: a light edge on all four sides and
// nearest-filtered text for that window. Call retro_ui_end_window() before End().
void retro_ui_begin_window(int32_t scale);

// The close button in the title bar, drawn with the pixel font's X instead of
// ImGui's line-drawn one. Begin() the window WITHOUT ImGui's (p_open =
// nullptr) and call this right after retro_ui_begin_window(). True when clicked.
bool retro_ui_close_button();
void retro_ui_end_window();

// ImGui::Button, raised (the palette's button colors and edges) with its
// label drawn shadowed (with the palette's shadows). Same return value. Use
// inside the theme. _sized: a given size (0 = fit the label, like ImGui).
// A scrolling list in a field-colored box (BeginChild / EndChild): sunken
// in a theme whose buttons have a raised edge, no edge in a flat theme.
// retro_ui_end_list() always goes with retro_ui_begin_list().
bool retro_ui_begin_list(const char* id, float width, float height);   // 0 = the space left
void retro_ui_end_list();

// The raised edge of a button around the last item (e.g. a color swatch),
// in a theme whose buttons have one; nothing in a flat theme.
void retro_ui_raised_item_edge();

bool retro_ui_button(const char* label);
bool retro_ui_button_sized(const char* label, float width, float height);

// A choice among a few values as a row of buttons (not a combo): the chosen
// one stays pressed in (sunken, the highlight color, its label down a pixel),
// the others raised. True when another one was clicked (*current changed).
bool retro_ui_choice_buttons(const char* id, const char* const* labels, uint32_t count, uint32_t* current);

// The theme's mouse cursor, when the palette has one: hides the system's and
// draws ours at the mouse. Call once per frame, after the UI; `visible`: the
// UI is up (while a game plays without it, the theme cursor isn't drawn).
void retro_ui_cursor(int32_t scale, bool visible);

// ImGui::Checkbox with the pixel font's check mark instead of ImGui's
// line-drawn one. Same return value. Use inside the theme.
bool retro_ui_checkbox(const char* label, bool* value);

// Tooltips that wrap at about RETRO_UI_TOOLTIP_CHARACTERS characters
// instead of running in one line past the window. Use inside the theme.
// retro_ui_item_tooltip: when the last item is hovered (ImGui::SetItemTooltip);
// retro_ui_tooltip: now (ImGui::SetTooltip, the caller checked hovering).
constexpr float RETRO_UI_TOOLTIP_CHARACTERS = 40.0f;
void retro_ui_item_tooltip(const char* text);
void retro_ui_tooltip(const char* text);

// Text inside a field (InputText, InputInt...) in the palette's field color.
void retro_ui_push_field_text();
void retro_ui_pop_field_text();

// ImGui::SeparatorText in the palette's section color.
void retro_ui_separator_text(const char* label);

// ImGui::CollapsingHeader with the pixel font's arrows (right = closed, down
// = open) instead of ImGui's line-drawn triangle. Same return value. Use
// inside the theme.
bool retro_ui_collapsing_header(const char* label, int flags);

// ImGui::BeginCombo with the pixel font's down arrow instead of ImGui's own
// triangle. Same return value and EndCombo() rule. Use inside the theme.
bool retro_ui_begin_combo(const char* label, const char* preview);

// Text that must stand out (the row being bound): the section title color
// (yellow in ZSNES), or the highlight color where that is the text color.
void retro_ui_push_section_text();
void retro_ui_pop_section_text();
uint32_t retro_ui_attention_color();   // that color as ImGui's packed color (ImU32)

// Tabs by PAGE LEFT / PAGE RIGHT (L / R on a gamepad, Page Up / Page Down on
// the keyboard): piccolo hands this frame's request (-1, +1 or 0), and each
// tab bar asks for the tab to select now. Only the focused window acts on
// it; the request is used once. Returns the index or -1.
void retro_ui_set_page_request(int delta);
int  retro_ui_tab_switch(int current, int count);

// Every window's vertical scroll bar in the theme's style (a sunken track,
// a raised grip), drawn over ImGui's. Call once a frame after all windows,
// before the frame is drawn.
void retro_ui_scrollbars(int32_t scale);

// With the palette's shadows: a drop shadow under every letter ImGui will
// draw, `scale` pixels down and right, in the text shadow color. For
// debug_ui_set_draw_data_hook(), between finishing the frame and drawing it.
struct ImDrawData;
void retro_ui_text_shadows(ImDrawData* draw_data, int32_t scale);
