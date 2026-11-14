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
#include "settings.h"
#include "viewport.h"

// Keys (file keys: stable, never translated).
constexpr const char* PICCOLO_LANGUAGE = "language";
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
constexpr const char* PICCOLO_LATENCY_FRAMES_IN_FLIGHT = "latency_frames_in_flight";
constexpr const char* PICCOLO_LATENCY_WAIT_FOR_PRESENT = "latency_wait_for_present";
constexpr const char* PICCOLO_LATENCY_FRAME_DELAY = "latency_frame_delay";
constexpr const char* PICCOLO_DISPLAY_FULLSCREEN = "display_fullscreen";
constexpr const char* PICCOLO_DISPLAY_FULLSCREEN_TYPE = "display_fullscreen_type";
constexpr const char* PICCOLO_WINDOW_MODE = "window_mode";
constexpr const char* PICCOLO_WINDOW_ASPECT = "window_aspect";
constexpr const char* PICCOLO_WINDOW_HEIGHT = "window_height";
constexpr const char* PICCOLO_WINDOW_SCALE = "window_scale";
constexpr const char* PICCOLO_WINDOW_REMEMBER = "window_remember";
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

// The Sound settings as audio_output's config.
void piccolo_settings_audio(const Settings* settings, AudioOutputConfig* config);

// Fixed window mode: the window's aspect and height from the settings.
double piccolo_settings_window_aspect(const Settings* settings);
uint32_t piccolo_settings_window_height(const Settings* settings);
