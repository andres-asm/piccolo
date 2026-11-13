#pragma once

// piccolo's settings: the descriptor table for Multitap's layered settings
// store (settings.h) and what the rest of piccolo reads from it.
//
// Stored in <settings>/piccolo.cfg (app_paths.h). Labels are English for now;
// they become string IDs with localization (plan-settings step 9).

#include <cstdint>

#include "frame_timing.h"
#include "settings.h"
#include "viewport.h"

// Keys (file keys: stable, never translated).
constexpr const char* PICCOLO_PAUSE_IN_MENU = "pause_in_menu";
constexpr const char* PICCOLO_PAUSE_ON_FOCUS_LOSS = "pause_on_focus_loss";
constexpr const char* PICCOLO_SCREENSAVER_WHEN_PAUSED = "screensaver_when_paused";
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
constexpr const char* PICCOLO_DISPLAY_FULLSCREEN = "display_fullscreen";
constexpr const char* PICCOLO_DISPLAY_FULLSCREEN_TYPE = "display_fullscreen_type";
constexpr const char* PICCOLO_WINDOW_MODE = "window_mode";
constexpr const char* PICCOLO_WINDOW_ASPECT = "window_aspect";
constexpr const char* PICCOLO_WINDOW_HEIGHT = "window_height";
constexpr const char* PICCOLO_WINDOW_SCALE = "window_scale";
constexpr const char* PICCOLO_WINDOW_REMEMBER = "window_remember";

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

// Fixed window mode: the window's aspect and height from the settings.
double piccolo_settings_window_aspect(const Settings* settings);
uint32_t piccolo_settings_window_height(const Settings* settings);
