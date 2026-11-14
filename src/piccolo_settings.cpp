#include "piccolo_settings.h"

#include "piccolo_text.h"

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
    {.key = PICCOLO_SCREENSAVER_WHEN_PAUSED, .type = SettingType::boolean, .default_value = "true",
     .label = text_id(Str::setting_screensaver),
     .help = text_id(Str::setting_screensaver_help)},
    {.key = PICCOLO_GAME_MODE, .type = SettingType::boolean, .default_value = "true",
     .label = text_id(Str::setting_game_mode), .help = text_id(Str::setting_game_mode_help)},
    {.key = PICCOLO_PATH_CORES, .type = SettingType::path, .default_value = "", .label = text_id(Str::setting_path_cores),
     .help = text_id(Str::setting_path_cores_help)},
    {.key = PICCOLO_PATH_SYSTEM, .type = SettingType::path, .default_value = "", .label = text_id(Str::setting_path_system),
     .help = text_id(Str::setting_path_system_help)},
    {.key = PICCOLO_PATH_SAVES, .type = SettingType::path, .default_value = "", .label = text_id(Str::setting_path_saves),
     .help = text_id(Str::setting_path_saves_help)},
    {.key = PICCOLO_PATH_STATES, .type = SettingType::path, .default_value = "", .label = text_id(Str::setting_path_states),
     .help = text_id(Str::setting_path_states_help)},

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
    {.key = PICCOLO_LATENCY_WAIT_FOR_PRESENT, .type = SettingType::boolean, .default_value = "false",
     .overridable = true, .label = text_id(Str::setting_wait_for_present),
     .help = text_id(Str::setting_wait_for_present_help)},
    {.key = PICCOLO_LATENCY_FRAME_DELAY, .type = SettingType::integer, .default_value = "0", .overridable = true,
     .min = 0, .max = 12, .label = text_id(Str::setting_frame_delay),
     .help = text_id(Str::setting_frame_delay_help)},
    {.key = PICCOLO_LATENCY_FRAME_DELAY_AUTO, .type = SettingType::boolean, .default_value = "false",
     .overridable = true, .label = text_id(Str::setting_frame_delay_auto),
     .help = text_id(Str::setting_frame_delay_auto_help)},

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
