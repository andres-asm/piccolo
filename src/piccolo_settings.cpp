#include "piccolo_settings.h"

#include "host_fs.h"
#include "piccolo_text.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <SDL3/SDL.h>

// ---------------------------------------------------------------------------
// Enum values (key in the file, label on screen)
// ---------------------------------------------------------------------------

static const SettingEnumValue ASPECT_VALUES[] = {
    {"core", text_id(Str::value_aspect_core)},
    {"square_pixels", text_id(Str::value_aspect_square_pixels)},
    {"4:3"},
    {"16:9"},
    {"16:10"},
    {"8:7"},
    {"3:2"},
    {"5:4"},
    {"stretch", text_id(Str::value_aspect_stretch)},
};

static const SettingEnumValue SCALING_VALUES[] = {
    {"full", text_id(Str::value_scaling_full)},
    {"integer_under", text_id(Str::value_scaling_integer_under)},
    {"integer_over", text_id(Str::value_scaling_integer_over)},
    {"auto", text_id(Str::value_scaling_auto)},
};

static const SettingEnumValue FILTER_VALUES[] = {
    {"nearest", text_id(Str::value_filter_nearest)},
    {"bilinear", text_id(Str::value_filter_bilinear)},
    {"sharp_bilinear", text_id(Str::value_filter_sharp_bilinear)},
};

static const SettingEnumValue COLOR_PROFILE_VALUES[] = {
    {"none", text_id(Str::value_color_profile_none)},
    {"gba", text_id(Str::value_color_profile_gba)},
    {"gba_sp101", text_id(Str::value_color_profile_gba_sp101)},
    {"gba_nso", text_id(Str::value_color_profile_gba_nso)},
    {"gbc", text_id(Str::value_color_profile_gbc)},
    {"gbc_gambatte", text_id(Str::value_color_profile_gbc_gambatte)},
    {"gb_micro", text_id(Str::value_color_profile_gb_micro)},
    {"nds", text_id(Str::value_color_profile_nds)},
    {"nds_lite", text_id(Str::value_color_profile_nds_lite)},
    {"psp", text_id(Str::value_color_profile_psp)},
    {"switch_oled", text_id(Str::value_color_profile_switch_oled)},
    {"palm", text_id(Str::value_color_profile_palm)},
    {"vba", text_id(Str::value_color_profile_vba)},
    {"ntsc", text_id(Str::value_color_profile_ntsc)},
};
constexpr uint32_t COLOR_PROFILE_COUNT = sizeof(COLOR_PROFILE_VALUES) / sizeof(COLOR_PROFILE_VALUES[0]);

static const SettingEnumValue POSITION_VALUES[] = {
    {"top_left", text_id(Str::value_position_top_left)},       {"top", text_id(Str::value_position_top)},       {"top_right", text_id(Str::value_position_top_right)},
    {"left", text_id(Str::value_position_left)},               {"center", text_id(Str::value_position_center)}, {"right", text_id(Str::value_position_right)},
    {"bottom_left", text_id(Str::value_position_bottom_left)}, {"bottom", text_id(Str::value_position_bottom)}, {"bottom_right", text_id(Str::value_position_bottom_right)},
};

static const SettingEnumValue TIMING_VALUES[] = {
    {"vsync", text_id(Str::value_timing_vsync)},
    {"exact", text_id(Str::value_timing_exact)},
};

static const SettingEnumValue LANGUAGE_VALUES[] = {
    {"auto", text_id(Str::value_language_auto)},
    {"en", text_id(Str::value_language_english)},
    {"es", text_id(Str::value_language_spanish)},
};
constexpr uint32_t LANGUAGE_COUNT = sizeof(LANGUAGE_VALUES) / sizeof(LANGUAGE_VALUES[0]);

static const SettingEnumValue FRAMES_IN_FLIGHT_VALUES[] = {
    {"2", text_id(Str::value_frames_2)},
    {"1", text_id(Str::value_frames_1)},
};
constexpr uint32_t FRAMES_IN_FLIGHT_COUNT = sizeof(FRAMES_IN_FLIGHT_VALUES) / sizeof(FRAMES_IN_FLIGHT_VALUES[0]);

static const SettingEnumValue FULLSCREEN_TYPE_VALUES[] = {
    {"borderless", text_id(Str::value_fullscreen_borderless)},
    {"exclusive", text_id(Str::value_fullscreen_exclusive)},
};

static const SettingEnumValue WINDOW_MODE_VALUES[] = {
    {"fixed", text_id(Str::value_window_fixed)},
    {"dynamic", text_id(Str::value_window_dynamic)},
};

static const SettingEnumValue WINDOW_ASPECT_VALUES[] = {
    {"16:9"},
    {"16:10"},
    {"4:3"},
};
static const double WINDOW_ASPECTS[] = {16.0 / 9.0, 16.0 / 10.0, 4.0 / 3.0};

static const SettingEnumValue WINDOW_HEIGHT_VALUES[] = {
    {"720"},
    {"900"},
    {"1080"},
    {"1200"},
    {"1440"},
};
static const uint32_t WINDOW_HEIGHTS[] = {720, 900, 1080, 1200, 1440};

constexpr uint32_t FULLSCREEN_TYPE_COUNT = sizeof(FULLSCREEN_TYPE_VALUES) / sizeof(FULLSCREEN_TYPE_VALUES[0]);
constexpr uint32_t WINDOW_MODE_COUNT = sizeof(WINDOW_MODE_VALUES) / sizeof(WINDOW_MODE_VALUES[0]);
constexpr uint32_t WINDOW_ASPECT_COUNT = sizeof(WINDOW_ASPECT_VALUES) / sizeof(WINDOW_ASPECT_VALUES[0]);
constexpr uint32_t WINDOW_HEIGHT_COUNT = sizeof(WINDOW_HEIGHT_VALUES) / sizeof(WINDOW_HEIGHT_VALUES[0]);

static const SettingEnumValue LATENCY_VALUES[] = {
    {"auto", text_id(Str::value_latency_auto)},
    {"40"},
    {"50"},
    {"60"},
    {"80"},
    {"100"},
    {"120"},
    {"160"},
    {"200"},
};
static const uint32_t LATENCIES[] = {RATE_CONTROL_AUTO_START_MS, 40, 50, 60, 80, 100, 120, 160, 200};
constexpr uint32_t LATENCY_COUNT = sizeof(LATENCY_VALUES) / sizeof(LATENCY_VALUES[0]);

static const SettingEnumValue QUALITY_VALUES[] = {
    {"fastest", text_id(Str::value_quality_fastest)},
    {"medium", text_id(Str::value_quality_medium)},
    {"best", text_id(Str::value_quality_best)},
};
static const AudioQuality QUALITIES[] = {AudioQuality::fastest, AudioQuality::medium, AudioQuality::best};
constexpr uint32_t QUALITY_COUNT = sizeof(QUALITY_VALUES) / sizeof(QUALITY_VALUES[0]);

constexpr uint32_t ASPECT_COUNT = sizeof(ASPECT_VALUES) / sizeof(ASPECT_VALUES[0]);
constexpr uint32_t SCALING_COUNT = sizeof(SCALING_VALUES) / sizeof(SCALING_VALUES[0]);
constexpr uint32_t FILTER_COUNT = sizeof(FILTER_VALUES) / sizeof(FILTER_VALUES[0]);
constexpr uint32_t POSITION_COUNT = sizeof(POSITION_VALUES) / sizeof(POSITION_VALUES[0]);
constexpr uint32_t TIMING_COUNT = sizeof(TIMING_VALUES) / sizeof(TIMING_VALUES[0]);

// ---------------------------------------------------------------------------
// The table
// ---------------------------------------------------------------------------

constexpr int32_t CROP_MAX = 128;   // core pixels per edge

static const SettingDescriptor SETTINGS[] = {
    // General (global-only)
    {.key = PICCOLO_LANGUAGE, .type = SettingType::enumeration, .default_value = "auto", .values = LANGUAGE_VALUES,
     .value_count = LANGUAGE_COUNT, .label = text_id(Str::setting_language),
     .help = text_id(Str::setting_language_help)},
    {.key = PICCOLO_PAUSE_IN_MENU, .type = SettingType::boolean, .default_value = "true",
     .label = text_id(Str::setting_pause_in_menu), .help = text_id(Str::setting_pause_in_menu_help)},
    {.key = PICCOLO_PAUSE_ON_FOCUS_LOSS, .type = SettingType::boolean, .default_value = "true",
     .label = text_id(Str::setting_pause_on_focus_loss), .help = text_id(Str::setting_pause_on_focus_loss_help)},
    {.key = PICCOLO_PAUSE_ON_DISCONNECT, .type = SettingType::boolean, .default_value = "true",
     .label = text_id(Str::setting_pause_on_disconnect), .help = text_id(Str::setting_pause_on_disconnect_help)},
    {.key = PICCOLO_SCREENSAVER_WHEN_PAUSED, .type = SettingType::boolean, .default_value = "true",
     .label = text_id(Str::setting_screensaver),
     .help = text_id(Str::setting_screensaver_help)},
    // Input (overridable per core / game; the bindings are per controller, in input.cfg)
    {.key = PICCOLO_INPUT_SHARE_PORT1, .type = SettingType::boolean, .default_value = "true", .overridable = true,
     .label = text_id(Str::setting_input_share_port1), .help = text_id(Str::setting_input_share_port1_help)},
    {.key = PICCOLO_INPUT_PRESS_TIME, .type = SettingType::real, .default_value = "5", .min_real = 1.0, .max_real = 15.0,
     .step_real = 0.5, .label = text_id(Str::setting_input_press_time), .help = text_id(Str::setting_input_press_time_help)},
    {.key = PICCOLO_INPUT_HOLD_TIME, .type = SettingType::real, .default_value = "0.5", .min_real = 0.1, .max_real = 5.0,
     .step_real = 0.1, .label = text_id(Str::setting_input_hold_time), .help = text_id(Str::setting_input_hold_time_help)},
    {.key = PICCOLO_INPUT_PORTS, .type = SettingType::integer, .default_value = "4", .overridable = true, .min = 1, .max = 8,
     .label = text_id(Str::setting_input_ports), .help = text_id(Str::setting_input_ports_help)},
    {.key = PICCOLO_INPUT_PORT_DEVICES[0], .type = SettingType::integer, .default_value = "1", .overridable = true,
     .min = 0, .max = 0x7fffffff, .label = text_id(Str::setting_input_port_device), .help = text_id(Str::setting_input_port_device_help)},
    {.key = PICCOLO_INPUT_PORT_DEVICES[1], .type = SettingType::integer, .default_value = "1", .overridable = true,
     .min = 0, .max = 0x7fffffff, .label = text_id(Str::setting_input_port_device), .help = text_id(Str::setting_input_port_device_help)},
    {.key = PICCOLO_INPUT_PORT_DEVICES[2], .type = SettingType::integer, .default_value = "1", .overridable = true,
     .min = 0, .max = 0x7fffffff, .label = text_id(Str::setting_input_port_device), .help = text_id(Str::setting_input_port_device_help)},
    {.key = PICCOLO_INPUT_PORT_DEVICES[3], .type = SettingType::integer, .default_value = "1", .overridable = true,
     .min = 0, .max = 0x7fffffff, .label = text_id(Str::setting_input_port_device), .help = text_id(Str::setting_input_port_device_help)},
    {.key = PICCOLO_INPUT_PORT_DEVICES[4], .type = SettingType::integer, .default_value = "1", .overridable = true,
     .min = 0, .max = 0x7fffffff, .label = text_id(Str::setting_input_port_device), .help = text_id(Str::setting_input_port_device_help)},
    {.key = PICCOLO_INPUT_PORT_DEVICES[5], .type = SettingType::integer, .default_value = "1", .overridable = true,
     .min = 0, .max = 0x7fffffff, .label = text_id(Str::setting_input_port_device), .help = text_id(Str::setting_input_port_device_help)},
    {.key = PICCOLO_INPUT_PORT_DEVICES[6], .type = SettingType::integer, .default_value = "1", .overridable = true,
     .min = 0, .max = 0x7fffffff, .label = text_id(Str::setting_input_port_device), .help = text_id(Str::setting_input_port_device_help)},
    {.key = PICCOLO_INPUT_PORT_DEVICES[7], .type = SettingType::integer, .default_value = "1", .overridable = true,
     .min = 0, .max = 0x7fffffff, .label = text_id(Str::setting_input_port_device), .help = text_id(Str::setting_input_port_device_help)},
    {.key = PICCOLO_INPUT_PORT_ADAPTERS[0], .type = SettingType::integer, .default_value = "0", .overridable = true,
     .min = -1, .max = 0x7fffffff, .label = text_id(Str::setting_input_port_adapter), .help = text_id(Str::setting_input_port_adapter_help)},
    {.key = PICCOLO_INPUT_PORT_ADAPTERS[1], .type = SettingType::integer, .default_value = "0", .overridable = true,
     .min = -1, .max = 0x7fffffff, .label = text_id(Str::setting_input_port_adapter), .help = text_id(Str::setting_input_port_adapter_help)},
    {.key = PICCOLO_INPUT_PORT_ADAPTERS[2], .type = SettingType::integer, .default_value = "0", .overridable = true,
     .min = -1, .max = 0x7fffffff, .label = text_id(Str::setting_input_port_adapter), .help = text_id(Str::setting_input_port_adapter_help)},
    {.key = PICCOLO_INPUT_PORT_ADAPTERS[3], .type = SettingType::integer, .default_value = "0", .overridable = true,
     .min = -1, .max = 0x7fffffff, .label = text_id(Str::setting_input_port_adapter), .help = text_id(Str::setting_input_port_adapter_help)},
    {.key = PICCOLO_GAME_MODE, .type = SettingType::boolean, .default_value = "true",
     .label = text_id(Str::setting_game_mode), .help = text_id(Str::setting_game_mode_help)},
    // Network (CONFIG > NETWORK); 55420 = NET_DEFAULT_PORT.
    {.key = PICCOLO_NET_ENABLED, .type = SettingType::boolean, .default_value = "false",
     .label = text_id(Str::setting_net_enabled), .help = text_id(Str::setting_net_enabled_help)},
    {.key = PICCOLO_NET_PORT, .type = SettingType::integer, .default_value = "55420", .min = 1024, .max = 65535,
     .label = text_id(Str::setting_net_port), .help = text_id(Str::setting_net_port_help)},
    {.key = PICCOLO_NET_NAME, .type = SettingType::text, .default_value = "PICCOLO",
     .label = text_id(Str::setting_net_name), .help = text_id(Str::setting_net_name_help)},
    {.key = PICCOLO_PATH_CORES, .type = SettingType::path, .default_value = "", .label = text_id(Str::setting_path_cores),
     .help = text_id(Str::setting_path_cores_help)},
    {.key = PICCOLO_PATH_SYSTEM, .type = SettingType::path, .default_value = "", .label = text_id(Str::setting_path_system),
     .help = text_id(Str::setting_path_system_help)},
    {.key = PICCOLO_PATH_SAVES, .type = SettingType::path, .default_value = "", .label = text_id(Str::setting_path_saves),
     .help = text_id(Str::setting_path_saves_help)},
    {.key = PICCOLO_PATH_STATES, .type = SettingType::path, .default_value = "", .label = text_id(Str::setting_path_states),
     .help = text_id(Str::setting_path_states_help)},
    {.key = PICCOLO_CORE_SERVER, .type = SettingType::text, .default_value = "https://cores.get-piccolo.app",
     .label = text_id(Str::setting_core_server), .help = text_id(Str::setting_core_server_help)},
    {.key = PICCOLO_CORE_SERVER_INSECURE, .type = SettingType::boolean, .default_value = "false",
     .label = text_id(Str::setting_core_server_insecure), .help = text_id(Str::setting_core_server_insecure_help)},
    {.key = PICCOLO_CORES_SHOW_NON_FREE, .type = SettingType::boolean, .default_value = "false",
     .label = text_id(Str::cores_show_non_free), .help = text_id(Str::cores_show_non_free_help)},
    {.key = PICCOLO_CORES_SHOW_LEGACY, .type = SettingType::boolean, .default_value = "false",
     .label = text_id(Str::cores_show_legacy_setting), .help = text_id(Str::cores_show_legacy_help)},
    {.key = PICCOLO_CORES_SHOW_EXPERIMENTAL, .type = SettingType::boolean, .default_value = "false",
     .label = text_id(Str::cores_show_experimental_setting), .help = text_id(Str::cores_show_experimental_help)},
    {.key = PICCOLO_PATH_GAMES[0], .type = SettingType::path, .default_value = "", .label = text_id(Str::setting_path_games),
     .help = text_id(Str::setting_path_games_help)},
    {.key = PICCOLO_PATH_GAMES[1], .type = SettingType::path, .default_value = "", .label = text_id(Str::setting_path_games),
     .help = text_id(Str::setting_path_games_help)},
    {.key = PICCOLO_PATH_GAMES[2], .type = SettingType::path, .default_value = "", .label = text_id(Str::setting_path_games),
     .help = text_id(Str::setting_path_games_help)},
    {.key = PICCOLO_PATH_GAMES[3], .type = SettingType::path, .default_value = "", .label = text_id(Str::setting_path_games),
     .help = text_id(Str::setting_path_games_help)},
    {.key = PICCOLO_PATH_GAMES[4], .type = SettingType::path, .default_value = "", .label = text_id(Str::setting_path_games),
     .help = text_id(Str::setting_path_games_help)},
    {.key = PICCOLO_PATH_GAMES[5], .type = SettingType::path, .default_value = "", .label = text_id(Str::setting_path_games),
     .help = text_id(Str::setting_path_games_help)},
    {.key = PICCOLO_PATH_GAMES[6], .type = SettingType::path, .default_value = "", .label = text_id(Str::setting_path_games),
     .help = text_id(Str::setting_path_games_help)},
    {.key = PICCOLO_PATH_GAMES[7], .type = SettingType::path, .default_value = "", .label = text_id(Str::setting_path_games),
     .help = text_id(Str::setting_path_games_help)},

    // Video: Picture (overridable per core / game)
    {.key = PICCOLO_VIDEO_ASPECT, .type = SettingType::enumeration, .default_value = "core", .overridable = true,
     .values = ASPECT_VALUES, .value_count = ASPECT_COUNT, .label = text_id(Str::setting_aspect),
     .help = text_id(Str::setting_aspect_help)},
    {.key = PICCOLO_VIDEO_SCALING, .type = SettingType::enumeration, .default_value = "auto", .overridable = true,
     .values = SCALING_VALUES, .value_count = SCALING_COUNT, .label = text_id(Str::setting_scaling),
     .help = text_id(Str::setting_scaling_help)},
    {.key = PICCOLO_VIDEO_MAX_OFFSCREEN, .type = SettingType::real, .default_value = "5", .overridable = true,
     .min_real = 0.0, .max_real = 50.0, .step_real = 0.5, .label = text_id(Str::setting_max_offscreen),
     .help = text_id(Str::setting_max_offscreen_help)},
    {.key = PICCOLO_VIDEO_FILTER, .type = SettingType::enumeration, .default_value = "nearest", .overridable = true,
     .values = FILTER_VALUES, .value_count = FILTER_COUNT, .label = text_id(Str::setting_filter),
     .help = text_id(Str::setting_filter_help)},
    {.key = PICCOLO_VIDEO_SHADER_PRESET, .type = SettingType::path, .default_value = "", .overridable = true,
     .label = text_id(Str::setting_shader_preset), .help = text_id(Str::setting_shader_preset_help)},

    // Image: pass 0's picture controls (overridable per core / game)
    {.key = PICCOLO_IMAGE_COLOR_PROFILE, .type = SettingType::enumeration, .default_value = "none", .overridable = true,
     .values = COLOR_PROFILE_VALUES, .value_count = COLOR_PROFILE_COUNT, .label = text_id(Str::setting_image_color_profile),
     .help = text_id(Str::setting_image_color_profile_help)},
    {.key = PICCOLO_IMAGE_PROFILE_STRENGTH, .type = SettingType::real, .default_value = "1", .overridable = true,
     .min_real = 0.0, .max_real = 1.0, .step_real = 0.05, .label = text_id(Str::setting_image_profile_strength),
     .help = text_id(Str::setting_image_profile_strength_help)},
    {.key = PICCOLO_IMAGE_BRIGHTNESS, .type = SettingType::real, .default_value = "0", .overridable = true,
     .min_real = -1.0, .max_real = 1.0, .step_real = 0.01, .label = text_id(Str::setting_image_brightness),
     .help = text_id(Str::setting_image_brightness_help)},
    {.key = PICCOLO_IMAGE_CONTRAST, .type = SettingType::real, .default_value = "1", .overridable = true,
     .min_real = 0.0, .max_real = 2.0, .step_real = 0.01, .label = text_id(Str::setting_image_contrast),
     .help = text_id(Str::setting_image_contrast_help)},
    {.key = PICCOLO_IMAGE_SATURATION, .type = SettingType::real, .default_value = "1", .overridable = true,
     .min_real = 0.0, .max_real = 2.0, .step_real = 0.01, .label = text_id(Str::setting_image_saturation),
     .help = text_id(Str::setting_image_saturation_help)},
    {.key = PICCOLO_IMAGE_HUE, .type = SettingType::real, .default_value = "0", .overridable = true,
     .min_real = -180.0, .max_real = 180.0, .step_real = 1.0, .label = text_id(Str::setting_image_hue),
     .help = text_id(Str::setting_image_hue_help)},
    {.key = PICCOLO_IMAGE_GAMMA, .type = SettingType::real, .default_value = "1", .overridable = true,
     .min_real = 0.5, .max_real = 2.0, .step_real = 0.01, .label = text_id(Str::setting_image_gamma),
     .help = text_id(Str::setting_image_gamma_help)},
    {.key = PICCOLO_IMAGE_BLACK_LEVEL, .type = SettingType::real, .default_value = "0", .overridable = true,
     .min_real = -0.5, .max_real = 0.5, .step_real = 0.01, .label = text_id(Str::setting_image_black_level),
     .help = text_id(Str::setting_image_black_level_help)},
    {.key = PICCOLO_IMAGE_TEMPERATURE, .type = SettingType::real, .default_value = "0", .overridable = true,
     .min_real = -1.0, .max_real = 1.0, .step_real = 0.01, .label = text_id(Str::setting_image_temperature),
     .help = text_id(Str::setting_image_temperature_help)},
    {.key = PICCOLO_IMAGE_GAIN_RED, .type = SettingType::real, .default_value = "1", .overridable = true,
     .min_real = 0.0, .max_real = 2.0, .step_real = 0.01, .label = text_id(Str::setting_image_gain_red),
     .help = text_id(Str::setting_image_gain_red_help)},
    {.key = PICCOLO_IMAGE_GAIN_GREEN, .type = SettingType::real, .default_value = "1", .overridable = true,
     .min_real = 0.0, .max_real = 2.0, .step_real = 0.01, .label = text_id(Str::setting_image_gain_green),
     .help = text_id(Str::setting_image_gain_green_help)},
    {.key = PICCOLO_IMAGE_GAIN_BLUE, .type = SettingType::real, .default_value = "1", .overridable = true,
     .min_real = 0.0, .max_real = 2.0, .step_real = 0.01, .label = text_id(Str::setting_image_gain_blue),
     .help = text_id(Str::setting_image_gain_blue_help)},

    {.key = PICCOLO_VIDEO_POSITION, .type = SettingType::enumeration, .default_value = "center", .overridable = true,
     .values = POSITION_VALUES, .value_count = POSITION_COUNT, .label = text_id(Str::setting_position),
     .help = text_id(Str::setting_position_help)},
    {.key = PICCOLO_VIDEO_CROP_TOP, .type = SettingType::integer, .default_value = "0", .overridable = true,
     .min = 0, .max = CROP_MAX, .label = text_id(Str::setting_crop_top), .help = text_id(Str::setting_crop_top_help)},
    {.key = PICCOLO_VIDEO_CROP_BOTTOM, .type = SettingType::integer, .default_value = "0", .overridable = true,
     .min = 0, .max = CROP_MAX, .label = text_id(Str::setting_crop_bottom), .help = text_id(Str::setting_crop_bottom_help)},
    {.key = PICCOLO_VIDEO_CROP_LEFT, .type = SettingType::integer, .default_value = "0", .overridable = true,
     .min = 0, .max = CROP_MAX, .label = text_id(Str::setting_crop_left), .help = text_id(Str::setting_crop_left_help)},
    {.key = PICCOLO_VIDEO_CROP_RIGHT, .type = SettingType::integer, .default_value = "0", .overridable = true,
     .min = 0, .max = CROP_MAX, .label = text_id(Str::setting_crop_right), .help = text_id(Str::setting_crop_right_help)},

    // Video: Sync
    {.key = PICCOLO_VIDEO_TIMING, .type = SettingType::enumeration, .default_value = "vsync", .overridable = true,
     .values = TIMING_VALUES, .value_count = TIMING_COUNT, .label = text_id(Str::setting_timing),
     .help = text_id(Str::setting_timing_help)},

    {.key = PICCOLO_VIDEO_CATCH_UP, .type = SettingType::boolean, .default_value = "true", .overridable = true,
     .label = text_id(Str::setting_catch_up), .help = text_id(Str::setting_catch_up_help)},

    // Video: Sync, low latency (overridable: a heavy core may not afford it)
    {.key = PICCOLO_LATENCY_FRAMES_IN_FLIGHT, .type = SettingType::enumeration, .default_value = "2",
     .overridable = true, .values = FRAMES_IN_FLIGHT_VALUES, .value_count = FRAMES_IN_FLIGHT_COUNT,
     .label = text_id(Str::setting_frames_in_flight),
     .help = text_id(Str::setting_frames_in_flight_help)},
    // On by default (Andrés, 2026-11-14): AUTO FRAME DELAY, also on, needs it.
    {.key = PICCOLO_LATENCY_WAIT_FOR_PRESENT, .type = SettingType::boolean, .default_value = "true",
     .overridable = true, .label = text_id(Str::setting_wait_for_present),
     .help = text_id(Str::setting_wait_for_present_help)},
    {.key = PICCOLO_LATENCY_FRAME_DELAY, .type = SettingType::integer, .default_value = "0", .overridable = true,
     .min = 0, .max = 12, .label = text_id(Str::setting_frame_delay),
     .help = text_id(Str::setting_frame_delay_help)},
    {.key = PICCOLO_LATENCY_FRAME_DELAY_AUTO, .type = SettingType::boolean, .default_value = "true",
     .overridable = true, .label = text_id(Str::setting_frame_delay_auto),
     .help = text_id(Str::setting_frame_delay_auto_help)},
    {.key = PICCOLO_LATENCY_RUN_AHEAD, .type = SettingType::integer, .default_value = "0", .overridable = true,
     .min = 0, .max = 4, .label = text_id(Str::setting_run_ahead), .help = text_id(Str::setting_run_ahead_help)},

    // Video: Display (global-only)
    {.key = PICCOLO_DISPLAY_FULLSCREEN, .type = SettingType::boolean, .default_value = "false", .label = text_id(Str::setting_fullscreen),
     .help = text_id(Str::setting_fullscreen_help)},
    {.key = PICCOLO_DISPLAY_FULLSCREEN_TYPE, .type = SettingType::enumeration, .default_value = "borderless",
     .values = FULLSCREEN_TYPE_VALUES, .value_count = FULLSCREEN_TYPE_COUNT, .label = text_id(Str::setting_fullscreen_type),
     .help = text_id(Str::setting_fullscreen_type_help)},
    {.key = PICCOLO_WINDOW_MODE, .type = SettingType::enumeration, .default_value = "fixed",
     .values = WINDOW_MODE_VALUES, .value_count = WINDOW_MODE_COUNT, .label = text_id(Str::setting_window_mode),
     .help = text_id(Str::setting_window_mode_help)},
    {.key = PICCOLO_WINDOW_ASPECT, .type = SettingType::enumeration, .default_value = "16:9",
     .values = WINDOW_ASPECT_VALUES, .value_count = WINDOW_ASPECT_COUNT, .label = text_id(Str::setting_window_aspect),
     .help = text_id(Str::setting_window_aspect_help)},
    {.key = PICCOLO_WINDOW_HEIGHT, .type = SettingType::enumeration, .default_value = "1080",
     .values = WINDOW_HEIGHT_VALUES, .value_count = WINDOW_HEIGHT_COUNT, .label = text_id(Str::setting_window_height),
     .help = text_id(Str::setting_window_height_help)},
    {.key = PICCOLO_WINDOW_SCALE, .type = SettingType::integer, .default_value = "3", .min = 1, .max = 10,
     .label = text_id(Str::setting_window_scale), .help = text_id(Str::setting_window_scale_help)},
    {.key = PICCOLO_WINDOW_REMEMBER, .type = SettingType::boolean, .default_value = "true",
     .label = text_id(Str::setting_window_remember),
     .help = text_id(Str::setting_window_remember_help)},

    // Interface (global-only): MENU TITLE BOXES and the palette; defaults =
    // the BLUE theme (retro_ui.cpp). The THEME presets set them all.
    {.key = PICCOLO_UI_MENU_BOXES, .type = SettingType::boolean, .default_value = "false",
     .label = text_id(Str::setting_ui_menu_boxes), .help = text_id(Str::setting_ui_menu_boxes_help)},
    {.key = PICCOLO_UI_SHADOWS, .type = SettingType::boolean, .default_value = "false",
     .label = text_id(Str::setting_ui_shadows), .help = text_id(Str::setting_ui_shadows_help)},
    {.key = PICCOLO_UI_COLOR_BACKGROUND, .type = SettingType::color, .default_value = "#000000",
     .label = text_id(Str::setting_ui_color_background), .help = text_id(Str::setting_ui_color_background_help)},
    {.key = PICCOLO_UI_COLOR_BAR, .type = SettingType::color, .default_value = "#34507C",
     .label = text_id(Str::setting_ui_color_bar), .help = text_id(Str::setting_ui_color_bar_help)},
    {.key = PICCOLO_UI_COLOR_BAR_CENTER, .type = SettingType::color, .default_value = "#34507C",
     .label = text_id(Str::setting_ui_color_bar_center), .help = text_id(Str::setting_ui_color_bar_center_help)},
    {.key = PICCOLO_UI_COLOR_BAR_LIGHT, .type = SettingType::color, .default_value = "#5878A8",
     .label = text_id(Str::setting_ui_color_bar_light), .help = text_id(Str::setting_ui_color_bar_light_help)},
    {.key = PICCOLO_UI_COLOR_BAR_DARK, .type = SettingType::color, .default_value = "#1A2840",
     .label = text_id(Str::setting_ui_color_bar_dark), .help = text_id(Str::setting_ui_color_bar_dark_help)},
    {.key = PICCOLO_UI_COLOR_MENU_BOX, .type = SettingType::color, .default_value = "#34507C",
     .label = text_id(Str::setting_ui_color_menu_box), .help = text_id(Str::setting_ui_color_menu_box_help)},
    {.key = PICCOLO_UI_COLOR_MENU_BOX_LIGHT, .type = SettingType::color, .default_value = "#5878A8",
     .label = text_id(Str::setting_ui_color_menu_box_light), .help = text_id(Str::setting_ui_color_menu_box_light_help)},
    {.key = PICCOLO_UI_COLOR_MENU_BOX_DARK, .type = SettingType::color, .default_value = "#5878A8",
     .label = text_id(Str::setting_ui_color_menu_box_dark), .help = text_id(Str::setting_ui_color_menu_box_dark_help)},
    {.key = PICCOLO_UI_COLOR_MENU, .type = SettingType::color, .default_value = "#2A4068",
     .label = text_id(Str::setting_ui_color_menu), .help = text_id(Str::setting_ui_color_menu_help)},
    {.key = PICCOLO_UI_COLOR_WINDOW, .type = SettingType::color, .default_value = "#2A4068",
     .label = text_id(Str::setting_ui_color_window), .help = text_id(Str::setting_ui_color_window_help)},
    {.key = PICCOLO_UI_COLOR_WINDOW_LIGHT, .type = SettingType::color, .default_value = "#5878A8",
     .label = text_id(Str::setting_ui_color_window_light), .help = text_id(Str::setting_ui_color_window_light_help)},
    {.key = PICCOLO_UI_COLOR_WINDOW_DARK, .type = SettingType::color, .default_value = "#101828",
     .label = text_id(Str::setting_ui_color_window_dark), .help = text_id(Str::setting_ui_color_window_dark_help)},
    {.key = PICCOLO_UI_COLOR_FIELD, .type = SettingType::color, .default_value = "#1E2E4C",
     .label = text_id(Str::setting_ui_color_field), .help = text_id(Str::setting_ui_color_field_help)},
    {.key = PICCOLO_UI_COLOR_FIELD_HOVER, .type = SettingType::color, .default_value = "#26385C",
     .label = text_id(Str::setting_ui_color_field_hover), .help = text_id(Str::setting_ui_color_field_hover_help)},
    {.key = PICCOLO_UI_COLOR_HIGHLIGHT, .type = SettingType::color, .default_value = "#7090C8",
     .label = text_id(Str::setting_ui_color_highlight), .help = text_id(Str::setting_ui_color_highlight_help)},
    {.key = PICCOLO_UI_COLOR_TEXT, .type = SettingType::color, .default_value = "#E0E6F0",
     .label = text_id(Str::setting_ui_color_text), .help = text_id(Str::setting_ui_color_text_help)},
    {.key = PICCOLO_UI_COLOR_TEXT_HIGHLIGHT, .type = SettingType::color, .default_value = "#FFFFFF",
     .label = text_id(Str::setting_ui_color_text_highlight), .help = text_id(Str::setting_ui_color_text_highlight_help)},
    {.key = PICCOLO_UI_COLOR_TEXT_DISABLED, .type = SettingType::color, .default_value = "#8090B0",
     .label = text_id(Str::setting_ui_color_text_disabled), .help = text_id(Str::setting_ui_color_text_disabled_help)},
    {.key = PICCOLO_UI_COLOR_TEXT_SHADOW, .type = SettingType::color, .default_value = "#101828",
     .label = text_id(Str::setting_ui_color_text_shadow), .help = text_id(Str::setting_ui_color_text_shadow_help)},

    {.key = PICCOLO_UI_COLOR_TITLE, .type = SettingType::color, .default_value = "#34507C",
     .label = text_id(Str::setting_ui_color_title), .help = text_id(Str::setting_ui_color_title_help)},
    {.key = PICCOLO_UI_COLOR_TITLE_BOTTOM, .type = SettingType::color, .default_value = "#34507C",
     .label = text_id(Str::setting_ui_color_title_bottom), .help = text_id(Str::setting_ui_color_title_bottom_help)},
    {.key = PICCOLO_UI_COLOR_CHECK, .type = SettingType::color, .default_value = "#FFFFFF",
     .label = text_id(Str::setting_ui_color_check), .help = text_id(Str::setting_ui_color_check_help)},
    {.key = PICCOLO_UI_COLOR_SECTION, .type = SettingType::color, .default_value = "#E0E6F0",
     .label = text_id(Str::setting_ui_color_section), .help = text_id(Str::setting_ui_color_section_help)},
    {.key = PICCOLO_UI_THEME_CURSOR, .type = SettingType::boolean, .default_value = "false",
     .label = text_id(Str::setting_ui_theme_cursor), .help = text_id(Str::setting_ui_theme_cursor_help)},
    {.key = PICCOLO_UI_COLOR_BUTTON, .type = SettingType::color, .default_value = "#34507C",
     .label = text_id(Str::setting_ui_color_button), .help = text_id(Str::setting_ui_color_button_help)},
    {.key = PICCOLO_UI_COLOR_BUTTON_BOTTOM, .type = SettingType::color, .default_value = "#34507C",
     .label = text_id(Str::setting_ui_color_button_bottom), .help = text_id(Str::setting_ui_color_button_bottom_help)},
    {.key = PICCOLO_UI_COLOR_BUTTON_LIGHT, .type = SettingType::color, .default_value = "#34507C",
     .label = text_id(Str::setting_ui_color_button_light), .help = text_id(Str::setting_ui_color_button_light_help)},
    {.key = PICCOLO_UI_COLOR_BUTTON_DARK, .type = SettingType::color, .default_value = "#34507C",
     .label = text_id(Str::setting_ui_color_button_dark), .help = text_id(Str::setting_ui_color_button_dark_help)},
    {.key = PICCOLO_UI_COLOR_BUTTON_TEXT, .type = SettingType::color, .default_value = "#E0E6F0",
     .label = text_id(Str::setting_ui_color_button_text), .help = text_id(Str::setting_ui_color_button_text_help)},
    {.key = PICCOLO_UI_COLOR_FIELD_TEXT, .type = SettingType::color, .default_value = "#E0E6F0",
     .label = text_id(Str::setting_ui_color_field_text), .help = text_id(Str::setting_ui_color_field_text_help)},
    {.key = PICCOLO_UI_COLOR_CURSOR, .type = SettingType::color, .default_value = "#8CB4F0",
     .label = text_id(Str::setting_ui_color_cursor), .help = text_id(Str::setting_ui_color_cursor_help)},
    {.key = PICCOLO_UI_COLOR_CURSOR_EDGE, .type = SettingType::color, .default_value = "#8CB4F0",
     .label = text_id(Str::setting_ui_color_cursor_edge), .help = text_id(Str::setting_ui_color_cursor_edge_help)},

    // Sound. Volume and latency overridable per core / game (plan-milestones,
    // milestone 2); the device and the advanced ones are global.
    {.key = PICCOLO_AUDIO_DEVICE, .type = SettingType::text, .default_value = "",
     .label = text_id(Str::setting_audio_device), .help = text_id(Str::setting_audio_device_help)},
    {.key = PICCOLO_AUDIO_VOLUME, .type = SettingType::integer, .default_value = "100", .overridable = true,
     .min = 0, .max = 100, .step = 5, .label = text_id(Str::setting_volume), .help = text_id(Str::setting_volume_help)},
    {.key = PICCOLO_AUDIO_MUTE, .type = SettingType::boolean, .default_value = "false",
     .label = text_id(Str::setting_mute), .help = text_id(Str::setting_mute_help)},
    {.key = PICCOLO_AUDIO_LATENCY, .type = SettingType::enumeration, .default_value = "auto", .overridable = true,
     .values = LATENCY_VALUES, .value_count = LATENCY_COUNT, .label = text_id(Str::setting_audio_latency),
     .help = text_id(Str::setting_audio_latency_help)},
    {.key = PICCOLO_AUDIO_RATE_CONTROL, .type = SettingType::boolean, .default_value = "true",
     .label = text_id(Str::setting_rate_control), .help = text_id(Str::setting_rate_control_help)},
    {.key = PICCOLO_AUDIO_MAX_DEVIATION, .type = SettingType::real, .default_value = "0.5", .min_real = 0.1,
     .max_real = 1.0, .step_real = 0.1, .label = text_id(Str::setting_max_deviation),
     .help = text_id(Str::setting_max_deviation_help)},
    {.key = PICCOLO_AUDIO_QUALITY, .type = SettingType::enumeration, .default_value = "medium",
     .values = QUALITY_VALUES, .value_count = QUALITY_COUNT, .label = text_id(Str::setting_resampler_quality),
     .help = text_id(Str::setting_resampler_quality_help)},
};

// Color profiles: a screen's colors as numbers for pass 0. Same order as
// COLOR_PROFILE_VALUES. The handheld ones are hunterk / Pokefan531's color
// shaders in RetroArch's slang-shaders (handheld/shaders/color/<name>.slang,
// public domain; their sRGB profile, "full white" where there's a choice):
// to linear with gamma_in, the matrix times the luminance, back with
// gamma_out. NTSC: NTSC 1953 primaries (illuminant C) to XYZ to sRGB, the
// standard matrices multiplied here, back with the sRGB curve at 2.4 (as
// RetroArch's ntsc-colors). Checked against those shaders with
// librashader-cli: at most 1/255 apart.
struct ColorProfile
{
    float gamma_in;
    float gamma_out;
    bool  srgb_curve;
    float luminance;
    float matrix[3][3];   // [output channel][input channel], R G B
};

static const ColorProfile COLOR_PROFILES[] = {
    {1.0f, 1.0f, false, 1.0f, {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}},   // NONE: unchanged
    {2.2f, 2.2f, false, 0.91f, {{0.905f, 0.195f, -0.1f}, {0.1f, 0.65f, 0.25f}, {0.1575f, 0.1425f, 0.7f}}},   // gba-color
    {2.2f, 2.2f, false, 0.935f, {{0.96f, 0.11f, -0.07f}, {0.0325f, 0.89f, 0.0775f}, {0.001f, -0.03f, 1.029f}}},   // sp101-color
    {2.2f, 2.2f, false, 1.0f, {{0.865f, 0.1225f, 0.0125f}, {0.0575f, 0.925f, 0.0125f}, {0.0575f, 0.1225f, 0.82f}}},   // NSO-gba-color
    {2.2f, 2.2f, false, 0.91f, {{0.905f, 0.195f, -0.1f}, {0.1f, 0.65f, 0.25f}, {0.1575f, 0.1425f, 0.7f}}},   // gbc-color
    {1.0f, 1.0f, false, 1.0f, {{13.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f}, {0.0f, 12.0f / 16.0f, 4.0f / 16.0f}, {3.0f / 16.0f, 2.0f / 16.0f, 11.0f / 16.0f}}},   // gbc-gambatte-color
    {2.2f, 2.2f, false, 0.9f, {{0.8025f, 0.31f, -0.1125f}, {0.1f, 0.6875f, 0.2125f}, {0.1225f, 0.1125f, 0.765f}}},   // gbMicro-color
    {2.2f, 2.2f, false, 0.905f, {{0.835f, 0.27f, -0.105f}, {0.1f, 0.6375f, 0.2625f}, {0.105f, 0.175f, 0.72f}}},   // nds-color
    {2.2f, 2.2f, false, 0.935f, {{0.93f, 0.14f, -0.07f}, {0.025f, 0.9f, 0.075f}, {0.008f, -0.03f, 1.022f}}},   // dslite-color
    {2.2f, 2.2f, false, 0.87f, {{0.965f, 0.185f, -0.15f}, {0.0425f, 0.79f, 0.1675f}, {0.0175f, 0.01f, 0.9725f}}},   // psp-color
    {2.2f, 2.2f, false, 1.0f, {{1.54f, -0.48f, -0.06f}, {-0.045f, 1.0625f, -0.0175f}, {-0.0225f, -0.075f, 1.1025f}}},   // SwitchOLED-color
    {2.2f, 2.2f, false, 1.0f, {{0.525f, 0.13f, -0.07f}, {0.0525f, 0.545f, 0.1975f}, {0.09f, 0.15f, 0.76f}}},   // palm-color
    {2.45f, 1.45f, false, 1.0f, {{0.73f, 0.27f, 0.0f}, {0.0825f, 0.6775f, 0.24f}, {0.0825f, 0.24f, 0.6775f}}},   // vba-color
    {2.2f, 2.4f, true, 1.0f, {{1.5071f, -0.3724f, -0.0832f}, {-0.0275f, 0.935f, 0.067f}, {-0.0272f, -0.0401f, 1.1679f}}},   // ntsc-colors
};
static_assert(sizeof(COLOR_PROFILES) / sizeof(COLOR_PROFILES[0]) == COLOR_PROFILE_COUNT, "one profile per value");

const char* const PICCOLO_PROFILE_PARAM_NAMES[PICCOLO_PROFILE_PARAM_COUNT] = {
    "PROFILE_GAMMA_IN", "PROFILE_GAMMA_OUT", "PROFILE_CURVE", "PROFILE_STRENGTH",
    "M00", "M01", "M02", "M10", "M11", "M12", "M20", "M21", "M22",
};

void piccolo_color_profile_params(const Settings* settings, float values[PICCOLO_PROFILE_PARAM_COUNT])
{
    const ColorProfile* profile = &COLOR_PROFILES[settings_get_enum(settings, PICCOLO_IMAGE_COLOR_PROFILE)];
    values[0] = profile->gamma_in;
    values[1] = profile->gamma_out;
    values[2] = profile->srgb_curve ? 1.0f : 0.0f;
    values[3] = static_cast<float>(settings_get_real(settings, PICCOLO_IMAGE_PROFILE_STRENGTH));
    for (uint32_t row = 0; row < 3; row++)
    {
        for (uint32_t column = 0; column < 3; column++)
        {
            values[4 + row * 3 + column] = profile->matrix[row][column] * profile->luminance;
        }
    }
}

const PiccoloPass0Control PICCOLO_PASS0_CONTROLS[PICCOLO_PASS0_CONTROL_COUNT] = {
    {PICCOLO_IMAGE_BRIGHTNESS, "BRIGHTNESS"},
    {PICCOLO_IMAGE_CONTRAST, "CONTRAST"},
    {PICCOLO_IMAGE_SATURATION, "SATURATION"},
    {PICCOLO_IMAGE_HUE, "HUE"},
    {PICCOLO_IMAGE_GAMMA, "GAMMA"},
    {PICCOLO_IMAGE_BLACK_LEVEL, "BLACK_LEVEL"},
    {PICCOLO_IMAGE_TEMPERATURE, "TEMPERATURE"},
    {PICCOLO_IMAGE_GAIN_RED, "GAIN_R"},
    {PICCOLO_IMAGE_GAIN_GREEN, "GAIN_G"},
    {PICCOLO_IMAGE_GAIN_BLUE, "GAIN_B"},
};

const SettingDescriptor* piccolo_settings_table()
{
    return SETTINGS;
}

uint32_t piccolo_settings_count()
{
    return sizeof(SETTINGS) / sizeof(SETTINGS[0]);
}

// ---------------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------------

void piccolo_settings_viewport(const Settings* settings, ViewportInput* input)
{
    static const ViewportAspect ASPECTS[] = {
        ViewportAspect::core,  ViewportAspect::square_pixels, ViewportAspect::fixed,
        ViewportAspect::fixed, ViewportAspect::fixed,         ViewportAspect::fixed,
        ViewportAspect::fixed, ViewportAspect::fixed,         ViewportAspect::stretch,
    };
    static const double FIXED[] = {0.0, 0.0, 4.0 / 3.0, 16.0 / 9.0, 16.0 / 10.0, 8.0 / 7.0, 3.0 / 2.0, 5.0 / 4.0, 0.0};
    static const ViewportScaling SCALINGS[] = {ViewportScaling::full, ViewportScaling::integer_under,
                                               ViewportScaling::integer_over, ViewportScaling::automatic};
    static const ViewportFilter FILTERS[] = {ViewportFilter::nearest, ViewportFilter::bilinear,
                                             ViewportFilter::sharp_bilinear};
    static const ViewportAnchor ANCHORS[] = {
        ViewportAnchor::top_left,    ViewportAnchor::top,    ViewportAnchor::top_right,
        ViewportAnchor::left,        ViewportAnchor::center, ViewportAnchor::right,
        ViewportAnchor::bottom_left, ViewportAnchor::bottom, ViewportAnchor::bottom_right,
    };

    uint32_t aspect = settings_get_enum(settings, PICCOLO_VIDEO_ASPECT);
    input->aspect = ASPECTS[aspect];
    input->fixed_aspect = FIXED[aspect];
    input->scaling = SCALINGS[settings_get_enum(settings, PICCOLO_VIDEO_SCALING)];
    input->max_offscreen_percent = settings_get_real(settings, PICCOLO_VIDEO_MAX_OFFSCREEN);
    input->filter = FILTERS[settings_get_enum(settings, PICCOLO_VIDEO_FILTER)];
    input->anchor = ANCHORS[settings_get_enum(settings, PICCOLO_VIDEO_POSITION)];
    input->crop.top = static_cast<uint32_t>(settings_get_int(settings, PICCOLO_VIDEO_CROP_TOP));
    input->crop.bottom = static_cast<uint32_t>(settings_get_int(settings, PICCOLO_VIDEO_CROP_BOTTOM));
    input->crop.left = static_cast<uint32_t>(settings_get_int(settings, PICCOLO_VIDEO_CROP_LEFT));
    input->crop.right = static_cast<uint32_t>(settings_get_int(settings, PICCOLO_VIDEO_CROP_RIGHT));
}

TimingMode piccolo_settings_timing(const Settings* settings)
{
    return settings_get_enum(settings, PICCOLO_VIDEO_TIMING) == 0 ? TimingMode::vsync : TimingMode::exact;
}

void piccolo_settings_game_folders(const Settings* settings, std::vector<std::string>* folders)
{
    folders->clear();
    for (uint32_t i = 0; i < PICCOLO_GAME_FOLDER_MAX; i++)
    {
        std::string folder = settings_get_text(settings, PICCOLO_PATH_GAMES[i]);
        if (folder.empty())
        {
            continue;   // a file edited by hand may have gaps: skipped
        }
        host_fs_forward_slashes(&folder[0]);
        folders->push_back(folder);
    }
}

bool piccolo_settings_remove_game_folder(Settings* settings, uint32_t index)
{
    if (index >= PICCOLO_GAME_FOLDER_MAX ||
        settings_get_layer(settings, SettingLayer::global, PICCOLO_PATH_GAMES[index]) == nullptr)
    {
        return false;
    }
    for (uint32_t i = index; i + 1 < PICCOLO_GAME_FOLDER_MAX; i++)
    {
        const char* next = settings_get_layer(settings, SettingLayer::global, PICCOLO_PATH_GAMES[i + 1]);
        if (next == nullptr || next[0] == '\0')
        {
            settings_reset(settings, SettingLayer::global, PICCOLO_PATH_GAMES[i]);
        }
        else
        {
            std::string moved = next;   // the next set below may move the text it points into
            settings_set(settings, SettingLayer::global, PICCOLO_PATH_GAMES[i], moved.c_str());
        }
    }
    settings_reset(settings, SettingLayer::global, PICCOLO_PATH_GAMES[PICCOLO_GAME_FOLDER_MAX - 1]);
    return true;
}

double piccolo_settings_window_aspect(const Settings* settings)
{
    return WINDOW_ASPECTS[settings_get_enum(settings, PICCOLO_WINDOW_ASPECT)];
}

uint32_t piccolo_settings_window_height(const Settings* settings)
{
    return WINDOW_HEIGHTS[settings_get_enum(settings, PICCOLO_WINDOW_HEIGHT)];
}

void piccolo_settings_audio(const Settings* settings, AudioOutputConfig* config)
{
    const char* device = settings_get_text(settings, PICCOLO_AUDIO_DEVICE);
    if (config->device != device)
    {
        config->device = device;   // only when it changed: no copy every loop
    }
    config->volume_percent = static_cast<uint32_t>(settings_get_int(settings, PICCOLO_AUDIO_VOLUME));
    config->muted = settings_get_bool(settings, PICCOLO_AUDIO_MUTE);
    uint32_t latency = settings_get_enum(settings, PICCOLO_AUDIO_LATENCY);
    config->automatic_latency = latency == 0;
    config->latency_ms = LATENCIES[latency];
    config->rate_control = settings_get_bool(settings, PICCOLO_AUDIO_RATE_CONTROL);
    config->max_deviation = settings_get_real(settings, PICCOLO_AUDIO_MAX_DEVIATION) / 100.0;
    config->quality = QUALITIES[settings_get_enum(settings, PICCOLO_AUDIO_QUALITY)];
}

uint32_t piccolo_settings_frames_in_flight(const Settings* settings)
{
    return settings_get_enum(settings, PICCOLO_LATENCY_FRAMES_IN_FLIGHT) == 1 ? 1u : 2u;
}

// Frames in flight (enum index), wait for present, frame delay (ms).
struct LatencyPreset
{
    Str         name;
    uint32_t    frames_in_flight_index;
    bool        wait_for_present;
    int32_t     frame_delay;
};

static const LatencyPreset LATENCY_PRESETS[PICCOLO_LATENCY_PRESET_COUNT] = {
    {Str::preset_off, 0, false, 0},
    {Str::preset_low, 1, true, 0},
    {Str::preset_lowest, 1, true, 4},
};

Str piccolo_settings_latency_preset_name(uint32_t preset)
{
    return preset < PICCOLO_LATENCY_PRESET_COUNT ? LATENCY_PRESETS[preset].name : Str::preset_custom;
}

uint32_t piccolo_settings_latency_preset(const Settings* settings, SettingLayer layer)
{
    const char* frames = settings_get_at(settings, layer, PICCOLO_LATENCY_FRAMES_IN_FLIGHT, nullptr);
    const char* wait = settings_get_at(settings, layer, PICCOLO_LATENCY_WAIT_FOR_PRESENT, nullptr);
    const char* delay = settings_get_at(settings, layer, PICCOLO_LATENCY_FRAME_DELAY, nullptr);
    uint32_t frames_index = frames != nullptr && std::strcmp(frames, "1") == 0 ? 1u : 0u;
    bool waits = wait != nullptr && std::strcmp(wait, "true") == 0;
    int32_t delay_ms = delay != nullptr ? static_cast<int32_t>(std::strtol(delay, nullptr, 10)) : 0;
    for (uint32_t i = 0; i < PICCOLO_LATENCY_PRESET_COUNT; i++)
    {
        const LatencyPreset* preset = &LATENCY_PRESETS[i];
        if (preset->frames_in_flight_index == frames_index && preset->wait_for_present == waits &&
            preset->frame_delay == delay_ms)
        {
            return i;
        }
    }
    return PICCOLO_LATENCY_PRESET_COUNT;
}

bool piccolo_settings_apply_latency_preset(Settings* settings, SettingLayer layer, uint32_t preset)
{
    if (preset >= PICCOLO_LATENCY_PRESET_COUNT)
    {
        return false;
    }
    const LatencyPreset* values = &LATENCY_PRESETS[preset];
    return settings_set_enum(settings, layer, PICCOLO_LATENCY_FRAMES_IN_FLIGHT, values->frames_in_flight_index) &&
           settings_set_bool(settings, layer, PICCOLO_LATENCY_WAIT_FOR_PRESENT, values->wait_for_present) &&
           settings_set_int(settings, layer, PICCOLO_LATENCY_FRAME_DELAY, values->frame_delay);
}

uint32_t piccolo_settings_language(const Settings* settings)
{
    uint32_t choice = settings_get_enum(settings, PICCOLO_LANGUAGE);   // 0 auto, 1 en, 2 es
    if (choice > 0)
    {
        return choice - 1;
    }
    int count = 0;
    SDL_Locale** locales = SDL_GetPreferredLocales(&count);
    uint32_t language = 0;
    for (int i = 0; locales != nullptr && i < count; i++)
    {
        const char* code = locales[i]->language;   // "es", "en", ...
        bool found = false;
        for (uint32_t j = 0; j < PICCOLO_LANGUAGE_COUNT; j++)
        {
            if (code != nullptr && std::strcmp(code, piccolo_language_code(j)) == 0)
            {
                language = j;
                found = true;
                break;
            }
        }
        if (found)
        {
            break;
        }
    }
    SDL_free(locales);
    return language;
}

// ---------------------------------------------------------------------------
// Interface
// ---------------------------------------------------------------------------

// The color settings, in RetroUiPalette's order (palette_color below).
static const char* const UI_COLOR_KEYS[] = {
    PICCOLO_UI_COLOR_BACKGROUND,
    PICCOLO_UI_COLOR_BAR,
    PICCOLO_UI_COLOR_BAR_CENTER,
    PICCOLO_UI_COLOR_BAR_LIGHT,
    PICCOLO_UI_COLOR_BAR_DARK,
    PICCOLO_UI_COLOR_MENU_BOX,
    PICCOLO_UI_COLOR_MENU_BOX_LIGHT,
    PICCOLO_UI_COLOR_MENU_BOX_DARK,
    PICCOLO_UI_COLOR_MENU,
    PICCOLO_UI_COLOR_WINDOW,
    PICCOLO_UI_COLOR_WINDOW_LIGHT,
    PICCOLO_UI_COLOR_WINDOW_DARK,
    PICCOLO_UI_COLOR_FIELD,
    PICCOLO_UI_COLOR_FIELD_HOVER,
    PICCOLO_UI_COLOR_HIGHLIGHT,
    PICCOLO_UI_COLOR_TEXT,
    PICCOLO_UI_COLOR_TEXT_HIGHLIGHT,
    PICCOLO_UI_COLOR_TEXT_DISABLED,
    PICCOLO_UI_COLOR_TEXT_SHADOW,
    PICCOLO_UI_COLOR_TITLE,
    PICCOLO_UI_COLOR_TITLE_BOTTOM,
    PICCOLO_UI_COLOR_CHECK,
    PICCOLO_UI_COLOR_SECTION,
    PICCOLO_UI_COLOR_BUTTON,
    PICCOLO_UI_COLOR_BUTTON_BOTTOM,
    PICCOLO_UI_COLOR_BUTTON_LIGHT,
    PICCOLO_UI_COLOR_BUTTON_DARK,
    PICCOLO_UI_COLOR_BUTTON_TEXT,
    PICCOLO_UI_COLOR_FIELD_TEXT,
    PICCOLO_UI_COLOR_CURSOR,
    PICCOLO_UI_COLOR_CURSOR_EDGE,
};
constexpr uint32_t UI_COLOR_COUNT = sizeof(UI_COLOR_KEYS) / sizeof(UI_COLOR_KEYS[0]);

// The palette's color number `index`, in UI_COLOR_KEYS's order.
static uint32_t* palette_color(RetroUiPalette* palette, uint32_t index)
{
    switch (index)
    {
    case 0:
        return &palette->background;
    case 1:
        return &palette->bar_face;
    case 2:
        return &palette->bar_center;
    case 3:
        return &palette->bar_light;
    case 4:
        return &palette->bar_dark;
    case 5:
        return &palette->menu_box_face;
    case 6:
        return &palette->menu_box_light;
    case 7:
        return &palette->menu_box_dark;
    case 8:
        return &palette->menu_face;
    case 9:
        return &palette->panel_face;
    case 10:
        return &palette->panel_light;
    case 11:
        return &palette->panel_dark;
    case 12:
        return &palette->field_face;
    case 13:
        return &palette->field_hover;
    case 14:
        return &palette->highlight;
    case 15:
        return &palette->text;
    case 16:
        return &palette->text_highlight;
    case 17:
        return &palette->text_disabled;
    case 18:
        return &palette->text_shadow;
    case 19:
        return &palette->title_top;
    case 20:
        return &palette->title_bottom;
    case 21:
        return &palette->check;
    case 22:
        return &palette->section;
    case 23:
        return &palette->button_face;
    case 24:
        return &palette->button_bottom;
    case 25:
        return &palette->button_light;
    case 26:
        return &palette->button_dark;
    case 27:
        return &palette->button_text;
    case 28:
        return &palette->field_text;
    case 29:
        return &palette->cursor;
    case 30:
        return &palette->cursor_edge;
    default:
        return &palette->text;
    }
}

uint32_t piccolo_settings_ui_color_count()
{
    return UI_COLOR_COUNT;
}

const char* piccolo_settings_ui_color_key(uint32_t index)
{
    return index < UI_COLOR_COUNT ? UI_COLOR_KEYS[index] : UI_COLOR_KEYS[0];
}

void piccolo_settings_palette(const Settings* settings, RetroUiPalette* palette)
{
    *palette = *retro_ui_theme_palette(RetroUiTheme::blue);
    palette->menu_boxes = settings_get_bool(settings, PICCOLO_UI_MENU_BOXES);
    palette->shadows = settings_get_bool(settings, PICCOLO_UI_SHADOWS);
    palette->theme_cursor = settings_get_bool(settings, PICCOLO_UI_THEME_CURSOR);
    for (uint32_t i = 0; i < UI_COLOR_COUNT; i++)
    {
        *palette_color(palette, i) = settings_get_color(settings, UI_COLOR_KEYS[i]);
    }
}

Str piccolo_settings_ui_theme_name(RetroUiTheme theme)
{
    switch (theme)
    {
    case RetroUiTheme::blue:
        return Str::theme_blue;
    case RetroUiTheme::purple:
        return Str::theme_purple;
    case RetroUiTheme::zsnes:
        return Str::theme_zsnes;
    case RetroUiTheme::black:
        return Str::theme_black;
    case RetroUiTheme::count:
        break;
    }
    return Str::preset_custom;
}

bool piccolo_settings_ui_palette_is(const Settings* settings, const RetroUiPalette* palette)
{
    RetroUiPalette wanted = *palette;   // palette_color() takes a non-const palette
    RetroUiPalette current = {};
    piccolo_settings_palette(settings, &current);
    bool same = palette->menu_boxes == current.menu_boxes && palette->shadows == current.shadows &&
                palette->theme_cursor == current.theme_cursor;
    for (uint32_t i = 0; same && i < UI_COLOR_COUNT; i++)
    {
        same = (*palette_color(&wanted, i) | 0xFF000000u) == (*palette_color(&current, i) | 0xFF000000u);
    }
    return same;
}

RetroUiTheme piccolo_settings_ui_theme(const Settings* settings)
{
    for (uint32_t t = 0; t < static_cast<uint32_t>(RetroUiTheme::count); t++)
    {
        if (piccolo_settings_ui_palette_is(settings, retro_ui_theme_palette(static_cast<RetroUiTheme>(t))))
        {
            return static_cast<RetroUiTheme>(t);
        }
    }
    return RetroUiTheme::count;
}

bool piccolo_settings_apply_ui_palette(Settings* settings, const RetroUiPalette* palette)
{
    RetroUiPalette colors = *palette;   // palette_color() takes a non-const palette
    bool ok = settings_set_bool(settings, SettingLayer::global, PICCOLO_UI_MENU_BOXES, palette->menu_boxes);
    ok = settings_set_bool(settings, SettingLayer::global, PICCOLO_UI_SHADOWS, palette->shadows) && ok;
    ok = settings_set_bool(settings, SettingLayer::global, PICCOLO_UI_THEME_CURSOR, palette->theme_cursor) && ok;
    for (uint32_t i = 0; i < UI_COLOR_COUNT; i++)
    {
        ok = settings_set_color(settings, SettingLayer::global, UI_COLOR_KEYS[i], *palette_color(&colors, i)) && ok;
    }
    return ok;
}

bool piccolo_settings_apply_ui_theme(Settings* settings, RetroUiTheme theme)
{
    if (theme == RetroUiTheme::count)
    {
        return false;
    }
    return piccolo_settings_apply_ui_palette(settings, retro_ui_theme_palette(theme));
}

// A theme file's true/false value; anything else keeps `fallback`.
static bool theme_file_bool(const SettingsFile* file, const char* key, bool fallback)
{
    const char* value = settings_file_get(file, "", key);
    if (value == nullptr)
    {
        return fallback;
    }
    if (std::strcmp(value, "true") == 0)
    {
        return true;
    }
    if (std::strcmp(value, "false") == 0)
    {
        return false;
    }
    SDL_Log("Theme: %s = %s is not true or false", key, value);
    return fallback;
}

static bool theme_name_before(const PiccoloThemeFile& a, const PiccoloThemeFile& b)
{
    return a.name < b.name;
}

// Adds `theme` to the list, or replaces the one of its kind with its name.
static void add_theme(std::vector<PiccoloThemeFile>* themes, const PiccoloThemeFile* theme)
{
    for (size_t t = 0; t < themes->size(); t++)
    {
        if ((*themes)[t].name == theme->name && (*themes)[t].custom == theme->custom)
        {
            (*themes)[t] = *theme;
            return;
        }
    }
    themes->push_back(*theme);
}

void piccolo_themes_load_directory(std::vector<PiccoloThemeFile>* themes, const char* directory)
{
    int count = 0;
    char** files = SDL_GlobDirectory(directory, "*.cfg", 0, &count);
    for (int f = 0; files != nullptr && f < count; f++)
    {
        std::string file_name = files[f];
        std::string path = std::string(directory) + "/" + file_name;
        SettingsFile file = {};
        if (!settings_file_load(&file, path.c_str()) || file.sections.empty())
        {
            continue;
        }

        PiccoloThemeFile theme = {};
        theme.path = path;
        theme.custom = file_name.size() > 11 && file_name.compare(file_name.size() - 11, 11, "-custom.cfg") == 0;
        theme.palette = *retro_ui_theme_palette(RetroUiTheme::blue);
        const char* name = settings_file_get(&file, "", "name");
        if (name != nullptr && name[0] != '\0')
        {
            theme.name = name;
        }
        else
        {
            theme.name = file_name.substr(0, file_name.size() - 4);   // without ".cfg"
            for (size_t i = 0; i < theme.name.size(); i++)
            {
                theme.name[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(theme.name[i])));
            }
        }
        theme.palette.menu_boxes = theme_file_bool(&file, PICCOLO_UI_MENU_BOXES, theme.palette.menu_boxes);
        theme.palette.shadows = theme_file_bool(&file, PICCOLO_UI_SHADOWS, theme.palette.shadows);
        theme.palette.theme_cursor = theme_file_bool(&file, PICCOLO_UI_THEME_CURSOR, theme.palette.theme_cursor);
        for (uint32_t i = 0; i < UI_COLOR_COUNT; i++)
        {
            const char* value = settings_file_get(&file, "", UI_COLOR_KEYS[i]);
            uint32_t color = 0;
            if (value == nullptr)
            {
                continue;
            }
            if (!settings_color_from_text(value, &color))
            {
                SDL_Log("Theme %s: %s = %s is not a #RRGGBB color", path.c_str(), UI_COLOR_KEYS[i], value);
                continue;
            }
            *palette_color(&theme.palette, i) = color | 0xFF000000u;
        }

        add_theme(themes, &theme);
        SDL_Log("Theme %s from %s", theme.name.c_str(), path.c_str());
    }
    SDL_free(files);
    std::sort(themes->begin(), themes->end(), theme_name_before);
}

bool piccolo_themes_save(const Settings* settings, const char* name, const char* directory,
                         std::vector<PiccoloThemeFile>* themes)
{
    PiccoloThemeFile theme = {};
    theme.name = name;
    theme.custom = true;
    piccolo_settings_palette(settings, &theme.palette);

    // The file: the one a theme with this name came from, if it is in `directory`...
    std::string folder = std::string(directory) + "/";
    for (size_t t = 0; t < themes->size(); t++)
    {
        const std::string& path = (*themes)[t].path;
        if ((*themes)[t].custom && (*themes)[t].name == theme.name && path.compare(0, folder.size(), folder) == 0)
        {
            theme.path = path;
        }
    }
    // ...else <name>-custom.cfg: lower case letters and digits, "-" between
    // words, a number after the name if that file is taken.
    if (theme.path.empty())
    {
        std::string stem;
        for (size_t i = 0; i < theme.name.size(); i++)
        {
            unsigned char c = static_cast<unsigned char>(theme.name[i]);
            if (std::isalnum(c))
            {
                stem.push_back(static_cast<char>(std::tolower(c)));
            }
            else if (!stem.empty() && stem.back() != '-')
            {
                stem.push_back('-');
            }
        }
        while (!stem.empty() && stem.back() == '-')
        {
            stem.pop_back();
        }
        if (stem.empty())
        {
            stem = "theme";
        }
        theme.path = folder + stem + "-custom.cfg";
        for (int n = 2; host_fs_exists(theme.path.c_str()); n++)
        {
            theme.path = folder + stem + "-" + std::to_string(n) + "-custom.cfg";
        }
    }

    SettingsFile file = {};
    bool ok = settings_file_set(&file, "", "name", theme.name.c_str());
    ok = settings_file_set(&file, "", PICCOLO_UI_MENU_BOXES, theme.palette.menu_boxes ? "true" : "false") && ok;
    ok = settings_file_set(&file, "", PICCOLO_UI_SHADOWS, theme.palette.shadows ? "true" : "false") && ok;
    ok = settings_file_set(&file, "", PICCOLO_UI_THEME_CURSOR, theme.palette.theme_cursor ? "true" : "false") && ok;
    for (uint32_t i = 0; i < UI_COLOR_COUNT; i++)
    {
        char color[16] = {};
        std::snprintf(color, sizeof(color), "#%06X", *palette_color(&theme.palette, i) & 0xFFFFFFu);
        ok = settings_file_set(&file, "", UI_COLOR_KEYS[i], color) && ok;
    }
    if (!ok || !host_fs_make_directories(directory) ||
        !settings_file_save(&file, theme.path.c_str(), "piccolo interface theme, saved from SETTINGS > INTERFACE.\n"
                                                         "The keys are explained in themes/example.cfg next to piccolo."))
    {
        SDL_Log("Couldn't save the theme %s to %s", theme.name.c_str(), theme.path.c_str());
        return false;
    }
    SDL_Log("Theme %s saved to %s", theme.name.c_str(), theme.path.c_str());
    add_theme(themes, &theme);
    std::sort(themes->begin(), themes->end(), theme_name_before);
    return true;
}

// The interface switches, as the palette has them.
static const char* const UI_BOOL_KEYS[] = {PICCOLO_UI_MENU_BOXES, PICCOLO_UI_SHADOWS, PICCOLO_UI_THEME_CURSOR};
constexpr uint32_t UI_BOOL_COUNT = static_cast<uint32_t>(sizeof(UI_BOOL_KEYS) / sizeof(UI_BOOL_KEYS[0]));

static bool palette_bool(const RetroUiPalette* palette, uint32_t index)
{
    switch (index)
    {
    case 0:
        return palette->menu_boxes;
    case 1:
        return palette->shadows;
    default:
        return palette->theme_cursor;
    }
}

bool piccolo_settings_complete_ui_theme(Settings* settings)
{
    // How many saved values each theme agrees with.
    uint32_t saved = 0;
    uint32_t best_score = 0;
    RetroUiTheme best = RetroUiTheme::blue;
    for (uint32_t t = 0; t < static_cast<uint32_t>(RetroUiTheme::count); t++)
    {
        RetroUiPalette theme = *retro_ui_theme_palette(static_cast<RetroUiTheme>(t));
        uint32_t score = 0;
        saved = 0;
        for (uint32_t i = 0; i < UI_COLOR_COUNT; i++)
        {
            const char* value = settings_get_layer(settings, SettingLayer::global, UI_COLOR_KEYS[i]);
            uint32_t color = 0;
            if (value != nullptr && settings_color_from_text(value, &color))
            {
                saved++;
                score += (color | 0xFF000000u) == (*palette_color(&theme, i) | 0xFF000000u) ? 1u : 0u;
            }
        }
        for (uint32_t i = 0; i < UI_BOOL_COUNT; i++)
        {
            if (settings_get_layer(settings, SettingLayer::global, UI_BOOL_KEYS[i]) != nullptr)
            {
                saved++;
                score += settings_get_bool(settings, UI_BOOL_KEYS[i]) == palette_bool(&theme, i) ? 1u : 0u;
            }
        }
        if (score > best_score)
        {
            best_score = score;
            best = static_cast<RetroUiTheme>(t);
        }
    }
    if (saved == 0)
    {
        return false;
    }
    RetroUiPalette palette = *retro_ui_theme_palette(best);
    bool changed = false;
    for (uint32_t i = 0; i < UI_COLOR_COUNT; i++)
    {
        if (settings_get_layer(settings, SettingLayer::global, UI_COLOR_KEYS[i]) == nullptr)
        {
            changed = settings_set_color(settings, SettingLayer::global, UI_COLOR_KEYS[i], *palette_color(&palette, i)) ||
                      changed;
        }
    }
    for (uint32_t i = 0; i < UI_BOOL_COUNT; i++)
    {
        if (settings_get_layer(settings, SettingLayer::global, UI_BOOL_KEYS[i]) == nullptr)
        {
            changed = settings_set_bool(settings, SettingLayer::global, UI_BOOL_KEYS[i], palette_bool(&palette, i)) || changed;
        }
    }
    return changed;
}

void piccolo_settings_input(const Settings* settings, InputSettings* input)
{
    input->keyboard_shares_port1 = settings_get_bool(settings, PICCOLO_INPUT_SHARE_PORT1);
    input->press_ms = static_cast<uint64_t>(settings_get_real(settings, PICCOLO_INPUT_PRESS_TIME) * 1000.0 + 0.5);
    input->hold_ms = static_cast<uint64_t>(settings_get_real(settings, PICCOLO_INPUT_HOLD_TIME) * 1000.0 + 0.5);
}
