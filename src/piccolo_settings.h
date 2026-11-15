#pragma once

// piccolo's settings: the descriptor table for Multitap's layered settings
// store (settings.h) and what the rest of piccolo reads from it.
//
// Stored in <settings>/piccolo.cfg (app_paths.h). Labels are English for now;
// they become string IDs with localization (plan-settings step 9).

#include <cstdint>

#include "audio_output.h"
#include "frame_timing.h"
#include "piccolo_text.h"
#include "retro_ui.h"
#include "settings.h"
#include "viewport.h"

// Keys (file keys: stable, never translated).
constexpr const char* PICCOLO_LANGUAGE = "language";
constexpr const char* PICCOLO_PAUSE_IN_MENU = "pause_in_menu";
constexpr const char* PICCOLO_PAUSE_ON_FOCUS_LOSS = "pause_on_focus_loss";
constexpr const char* PICCOLO_SCREENSAVER_WHEN_PAUSED = "screensaver_when_paused";
constexpr const char* PICCOLO_GAME_MODE = "game_mode";
constexpr const char* PICCOLO_PATH_CORES = "path_cores";
constexpr const char* PICCOLO_PATH_SYSTEM = "path_system";
constexpr const char* PICCOLO_PATH_SAVES = "path_saves";
constexpr const char* PICCOLO_PATH_STATES = "path_states";
constexpr const char* PICCOLO_VIDEO_ASPECT = "video_aspect";
constexpr const char* PICCOLO_VIDEO_SCALING = "video_scaling";
constexpr const char* PICCOLO_VIDEO_MAX_OFFSCREEN = "video_max_offscreen";
constexpr const char* PICCOLO_VIDEO_FILTER = "video_filter";
constexpr const char* PICCOLO_VIDEO_POSITION = "video_position";
constexpr const char* PICCOLO_VIDEO_CROP_TOP = "video_crop_top";
constexpr const char* PICCOLO_VIDEO_CROP_BOTTOM = "video_crop_bottom";
constexpr const char* PICCOLO_VIDEO_CROP_LEFT = "video_crop_left";
constexpr const char* PICCOLO_VIDEO_CROP_RIGHT = "video_crop_right";
constexpr const char* PICCOLO_VIDEO_TIMING = "video_timing";
constexpr const char* PICCOLO_LATENCY_FRAMES_IN_FLIGHT = "latency_frames_in_flight";
constexpr const char* PICCOLO_LATENCY_WAIT_FOR_PRESENT = "latency_wait_for_present";
constexpr const char* PICCOLO_LATENCY_FRAME_DELAY = "latency_frame_delay";
constexpr const char* PICCOLO_LATENCY_FRAME_DELAY_AUTO = "latency_frame_delay_auto";
constexpr const char* PICCOLO_VIDEO_CATCH_UP = "video_catch_up";
constexpr const char* PICCOLO_DISPLAY_FULLSCREEN = "display_fullscreen";
constexpr const char* PICCOLO_DISPLAY_FULLSCREEN_TYPE = "display_fullscreen_type";
constexpr const char* PICCOLO_WINDOW_MODE = "window_mode";
constexpr const char* PICCOLO_WINDOW_ASPECT = "window_aspect";
constexpr const char* PICCOLO_WINDOW_HEIGHT = "window_height";
constexpr const char* PICCOLO_WINDOW_SCALE = "window_scale";
constexpr const char* PICCOLO_WINDOW_REMEMBER = "window_remember";
constexpr const char* PICCOLO_UI_MODE = "ui_mode";   // 0 desktop (menu bar), 1 big picture (big_picture.h)
constexpr const char* PICCOLO_UI_MENU_BOXES = "ui_menu_boxes";
constexpr const char* PICCOLO_UI_SHADOWS = "ui_shadows";
constexpr const char* PICCOLO_UI_COLOR_BACKGROUND = "ui_color_background";
constexpr const char* PICCOLO_UI_COLOR_TITLE = "ui_color_title";
constexpr const char* PICCOLO_UI_COLOR_TITLE_BOTTOM = "ui_color_title_bottom";
constexpr const char* PICCOLO_UI_COLOR_CHECK = "ui_color_check";
constexpr const char* PICCOLO_UI_COLOR_SECTION = "ui_color_section";
constexpr const char* PICCOLO_UI_THEME_CURSOR = "ui_theme_cursor";
constexpr const char* PICCOLO_UI_COLOR_BUTTON = "ui_color_button";
constexpr const char* PICCOLO_UI_COLOR_BUTTON_BOTTOM = "ui_color_button_bottom";
constexpr const char* PICCOLO_UI_COLOR_BUTTON_LIGHT = "ui_color_button_light";
constexpr const char* PICCOLO_UI_COLOR_BUTTON_DARK = "ui_color_button_dark";
constexpr const char* PICCOLO_UI_COLOR_BUTTON_TEXT = "ui_color_button_text";
constexpr const char* PICCOLO_UI_COLOR_FIELD_TEXT = "ui_color_field_text";
constexpr const char* PICCOLO_UI_COLOR_CURSOR = "ui_color_cursor";
constexpr const char* PICCOLO_UI_COLOR_BAR = "ui_color_bar";
constexpr const char* PICCOLO_UI_COLOR_BAR_CENTER = "ui_color_bar_center";
constexpr const char* PICCOLO_UI_COLOR_BAR_LIGHT = "ui_color_bar_light";
constexpr const char* PICCOLO_UI_COLOR_BAR_DARK = "ui_color_bar_dark";
constexpr const char* PICCOLO_UI_COLOR_MENU_BOX = "ui_color_menu_box";
constexpr const char* PICCOLO_UI_COLOR_MENU_BOX_LIGHT = "ui_color_menu_box_light";
constexpr const char* PICCOLO_UI_COLOR_MENU_BOX_DARK = "ui_color_menu_box_dark";
constexpr const char* PICCOLO_UI_COLOR_MENU = "ui_color_menu";
constexpr const char* PICCOLO_UI_COLOR_WINDOW = "ui_color_window";
constexpr const char* PICCOLO_UI_COLOR_WINDOW_LIGHT = "ui_color_window_light";
constexpr const char* PICCOLO_UI_COLOR_WINDOW_DARK = "ui_color_window_dark";
constexpr const char* PICCOLO_UI_COLOR_FIELD = "ui_color_field";
constexpr const char* PICCOLO_UI_COLOR_FIELD_HOVER = "ui_color_field_hover";
constexpr const char* PICCOLO_UI_COLOR_HIGHLIGHT = "ui_color_highlight";
constexpr const char* PICCOLO_UI_COLOR_TEXT = "ui_color_text";
constexpr const char* PICCOLO_UI_COLOR_TEXT_HIGHLIGHT = "ui_color_text_highlight";
constexpr const char* PICCOLO_UI_COLOR_TEXT_DISABLED = "ui_color_text_disabled";
constexpr const char* PICCOLO_UI_COLOR_TEXT_SHADOW = "ui_color_text_shadow";
constexpr const char* PICCOLO_AUDIO_DEVICE = "audio_device";
constexpr const char* PICCOLO_AUDIO_VOLUME = "audio_volume";
constexpr const char* PICCOLO_AUDIO_MUTE = "audio_mute";
constexpr const char* PICCOLO_AUDIO_LATENCY = "audio_latency";
constexpr const char* PICCOLO_AUDIO_RATE_CONTROL = "audio_rate_control";
constexpr const char* PICCOLO_AUDIO_MAX_DEVIATION = "audio_max_deviation";
constexpr const char* PICCOLO_AUDIO_QUALITY = "audio_resampler_quality";

// Values of the enumerations above, by index.
constexpr uint32_t PICCOLO_FULLSCREEN_BORDERLESS = 0;
constexpr uint32_t PICCOLO_FULLSCREEN_EXCLUSIVE = 1;
constexpr uint32_t PICCOLO_WINDOW_FIXED = 0;
constexpr uint32_t PICCOLO_WINDOW_DYNAMIC = 1;

const SettingDescriptor* piccolo_settings_table();
uint32_t piccolo_settings_count();

// The Picture settings as viewport settings (frame and window sizes not touched).
void piccolo_settings_viewport(const Settings* settings, ViewportInput* input);

TimingMode piccolo_settings_timing(const Settings* settings);

// The language to show: an index for piccolo_language_code() (piccolo_text.h).
// "auto" picks the first of the system's preferred languages piccolo has,
// else English.
uint32_t piccolo_settings_language(const Settings* settings);

// Low latency: frames in flight (1 or 2).
uint32_t piccolo_settings_frames_in_flight(const Settings* settings);

// The low latency presets: they set the three latency settings at once.
// Index 0..2 = OFF, LOW, LOWEST; piccolo_settings_latency_preset() gives the
// one the current values (at `layer`) match, or 3 (CUSTOM).
constexpr uint32_t PICCOLO_LATENCY_PRESET_COUNT = 3;
Str piccolo_settings_latency_preset_name(uint32_t preset);
uint32_t piccolo_settings_latency_preset(const Settings* settings, SettingLayer layer);
bool piccolo_settings_apply_latency_preset(Settings* settings, SettingLayer layer, uint32_t preset);

// The Interface settings as the UI's palette.
void piccolo_settings_palette(const Settings* settings, RetroUiPalette* palette);

// The themes (RetroUiTheme): they set every interface color at once.
// piccolo_settings_ui_theme() gives the one the colors match, or
// RetroUiTheme::count (CUSTOM).
Str piccolo_settings_ui_theme_name(RetroUiTheme theme);
RetroUiTheme piccolo_settings_ui_theme(const Settings* settings);
bool piccolo_settings_apply_ui_theme(Settings* settings, RetroUiTheme theme);

// After loading: when the global settings set some interface colors but not
// others (a settings file from before those were added), the missing ones
// come from the theme the saved ones match best, so a new theme color
// doesn't fall back to PICCOLO's inside another theme. Does nothing when no
// interface setting is saved. True if it set anything.
bool piccolo_settings_complete_ui_theme(Settings* settings);

// The interface color settings in table order, for the Interface tab.
uint32_t piccolo_settings_ui_color_count();
const char* piccolo_settings_ui_color_key(uint32_t index);

// The Sound settings as audio_output's config.
void piccolo_settings_audio(const Settings* settings, AudioOutputConfig* config);

// Fixed window mode: the window's aspect and height from the settings.
double piccolo_settings_window_aspect(const Settings* settings);
uint32_t piccolo_settings_window_height(const Settings* settings);
