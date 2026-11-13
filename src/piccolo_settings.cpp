#include "piccolo_settings.h"

#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
// Enum values (key in the file, label on screen)
// ---------------------------------------------------------------------------

static const SettingEnumValue ASPECT_VALUES[] = {
    {"core", "CORE PROVIDED"},
    {"square_pixels", "SQUARE PIXELS (1:1 PAR)"},
    {"4:3", "4:3"},
    {"16:9", "16:9"},
    {"16:10", "16:10"},
    {"8:7", "8:7"},
    {"3:2", "3:2"},
    {"5:4", "5:4"},
    {"stretch", "STRETCH"},
};

static const SettingEnumValue SCALING_VALUES[] = {
    {"full", "FULL"},
    {"integer_under", "INTEGER (UNDER)"},
    {"integer_over", "INTEGER (OVER)"},
    {"auto", "AUTO"},
};

static const SettingEnumValue FILTER_VALUES[] = {
    {"nearest", "NEAREST"},
    {"bilinear", "BILINEAR"},
    {"sharp_bilinear", "SHARP BILINEAR"},
};

static const SettingEnumValue POSITION_VALUES[] = {
    {"top_left", "TOP LEFT"},       {"top", "TOP"},       {"top_right", "TOP RIGHT"},
    {"left", "LEFT"},               {"center", "CENTER"}, {"right", "RIGHT"},
    {"bottom_left", "BOTTOM LEFT"}, {"bottom", "BOTTOM"}, {"bottom_right", "BOTTOM RIGHT"},
};

static const SettingEnumValue TIMING_VALUES[] = {
    {"vsync", "VSYNC"},
    {"exact", "EXACT"},
};

static const SettingEnumValue FRAMES_IN_FLIGHT_VALUES[] = {
    {"2", "2 (SMOOTHER)"},
    {"1", "1 (LOWER LATENCY)"},
};
constexpr uint32_t FRAMES_IN_FLIGHT_COUNT = sizeof(FRAMES_IN_FLIGHT_VALUES) / sizeof(FRAMES_IN_FLIGHT_VALUES[0]);

static const SettingEnumValue FULLSCREEN_TYPE_VALUES[] = {
    {"borderless", "BORDERLESS"},
    {"exclusive", "EXCLUSIVE"},
};

static const SettingEnumValue WINDOW_MODE_VALUES[] = {
    {"fixed", "FIXED SIZE"},
    {"dynamic", "DYNAMIC (FOLLOWS THE GAME)"},
};

static const SettingEnumValue WINDOW_ASPECT_VALUES[] = {
    {"16:9", "16:9"},
    {"16:10", "16:10"},
    {"4:3", "4:3"},
};
static const double WINDOW_ASPECTS[] = {16.0 / 9.0, 16.0 / 10.0, 4.0 / 3.0};

static const SettingEnumValue WINDOW_HEIGHT_VALUES[] = {
    {"720", "720 LINES"},
    {"900", "900 LINES"},
    {"1080", "1080 LINES"},
    {"1200", "1200 LINES"},
    {"1440", "1440 LINES"},
};
static const uint32_t WINDOW_HEIGHTS[] = {720, 900, 1080, 1200, 1440};

constexpr uint32_t FULLSCREEN_TYPE_COUNT = sizeof(FULLSCREEN_TYPE_VALUES) / sizeof(FULLSCREEN_TYPE_VALUES[0]);
constexpr uint32_t WINDOW_MODE_COUNT = sizeof(WINDOW_MODE_VALUES) / sizeof(WINDOW_MODE_VALUES[0]);
constexpr uint32_t WINDOW_ASPECT_COUNT = sizeof(WINDOW_ASPECT_VALUES) / sizeof(WINDOW_ASPECT_VALUES[0]);
constexpr uint32_t WINDOW_HEIGHT_COUNT = sizeof(WINDOW_HEIGHT_VALUES) / sizeof(WINDOW_HEIGHT_VALUES[0]);

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
    {.key = PICCOLO_PAUSE_IN_MENU, .type = SettingType::boolean, .default_value = "true",
     .label = "PAUSE WHILE THE MENU IS OPEN", .help = "Off: the game keeps running behind the menu."},
    {.key = PICCOLO_PAUSE_ON_FOCUS_LOSS, .type = SettingType::boolean, .default_value = "true",
     .label = "PAUSE WHEN THE WINDOW LOSES FOCUS", .help = "Pause while another window is active."},
    {.key = PICCOLO_SCREENSAVER_WHEN_PAUSED, .type = SettingType::boolean, .default_value = "true",
     .label = "ALLOW SCREENSAVER WHILE PAUSED",
     .help = "While a game runs the screensaver is always blocked. On: it may start while paused or in the menu."},
    {.key = PICCOLO_PATH_CORES, .type = SettingType::path, .default_value = "", .label = "CORES",
     .help = "Folder with the cores. Empty: the cores folder in piccolo's folder."},
    {.key = PICCOLO_PATH_SYSTEM, .type = SettingType::path, .default_value = "", .label = "SYSTEM / BIOS",
     .help = "Folder for BIOS and other files cores ask for. Empty: the system folder in piccolo's folder."},
    {.key = PICCOLO_PATH_SAVES, .type = SettingType::path, .default_value = "", .label = "SAVES",
     .help = "Folder for save RAM. Empty: the saves folder in piccolo's folder."},
    {.key = PICCOLO_PATH_STATES, .type = SettingType::path, .default_value = "", .label = "STATES",
     .help = "Folder for savestates. Empty: the states folder in piccolo's folder."},

    // Video: Picture (overridable per core / game)
    {.key = PICCOLO_VIDEO_ASPECT, .type = SettingType::enumeration, .default_value = "core", .overridable = true,
     .values = ASPECT_VALUES, .value_count = ASPECT_COUNT, .label = "ASPECT RATIO",
     .help = "The shape of the whole picture. Square pixels: the frame's own width / height."},
    {.key = PICCOLO_VIDEO_SCALING, .type = SettingType::enumeration, .default_value = "auto", .overridable = true,
     .values = SCALING_VALUES, .value_count = SCALING_COUNT, .label = "SCALING",
     .help = "Integer sizes keep every pixel the same size. Auto: one size up if it goes past the window by "
             "at most the % below."},
    {.key = PICCOLO_VIDEO_MAX_OFFSCREEN, .type = SettingType::real, .default_value = "5", .overridable = true,
     .min_real = 0.0, .max_real = 50.0, .step_real = 0.5, .label = "AUTO: MAX OFFSCREEN",
     .help = "How much of the picture Auto may push past the window edges."},
    {.key = PICCOLO_VIDEO_FILTER, .type = SettingType::enumeration, .default_value = "nearest", .overridable = true,
     .values = FILTER_VALUES, .value_count = FILTER_COUNT, .label = "FILTER",
     .help = "Sharp bilinear: sharp pixels, soft only where a pixel lands between two screen pixels."},
    {.key = PICCOLO_VIDEO_POSITION, .type = SettingType::enumeration, .default_value = "center", .overridable = true,
     .values = POSITION_VALUES, .value_count = POSITION_COUNT, .label = "POSITION",
     .help = "Where the picture sits when it doesn't fill the window."},
    {.key = PICCOLO_VIDEO_CROP_TOP, .type = SettingType::integer, .default_value = "0", .overridable = true,
     .min = 0, .max = CROP_MAX, .label = "CROP TOP", .help = "Core pixels cut from the top edge."},
    {.key = PICCOLO_VIDEO_CROP_BOTTOM, .type = SettingType::integer, .default_value = "0", .overridable = true,
     .min = 0, .max = CROP_MAX, .label = "CROP BOTTOM", .help = "Core pixels cut from the bottom edge."},
    {.key = PICCOLO_VIDEO_CROP_LEFT, .type = SettingType::integer, .default_value = "0", .overridable = true,
     .min = 0, .max = CROP_MAX, .label = "CROP LEFT", .help = "Core pixels cut from the left edge."},
    {.key = PICCOLO_VIDEO_CROP_RIGHT, .type = SettingType::integer, .default_value = "0", .overridable = true,
     .min = 0, .max = CROP_MAX, .label = "CROP RIGHT", .help = "Core pixels cut from the right edge."},

    // Video: Sync
    {.key = PICCOLO_VIDEO_TIMING, .type = SettingType::enumeration, .default_value = "vsync", .overridable = true,
     .values = TIMING_VALUES, .value_count = TIMING_COUNT, .label = "TIMING",
     .help = "Vsync: one frame per display refresh (smooth; falls back to Exact when the rates are too far "
             "apart). Exact: the core's own speed."},

    // Video: Sync, low latency (overridable: a heavy core may not afford it)
    {.key = PICCOLO_LATENCY_FRAMES_IN_FLIGHT, .type = SettingType::enumeration, .default_value = "2",
     .overridable = true, .values = FRAMES_IN_FLIGHT_VALUES, .value_count = FRAMES_IN_FLIGHT_COUNT,
     .label = "FRAMES IN FLIGHT",
     .help = "How many frames the CPU may prepare ahead of the GPU. 1: less delay, but a slow frame shows."},
    {.key = PICCOLO_LATENCY_WAIT_FOR_PRESENT, .type = SettingType::boolean, .default_value = "false",
     .overridable = true, .label = "WAIT FOR PRESENT",
     .help = "Don't start the next frame until this one is on screen (or, where the GPU can't tell, until the GPU "
             "has finished it)."},
    {.key = PICCOLO_LATENCY_FRAME_DELAY, .type = SettingType::integer, .default_value = "0", .overridable = true,
     .min = 0, .max = 12, .label = "FRAME DELAY (MS)",
     .help = "Vsync timing: wait this long before running each frame, so input is read later. Too much and "
             "frames are missed: raise it slowly."},

    // Video: Display (global-only)
    {.key = PICCOLO_DISPLAY_FULLSCREEN, .type = SettingType::boolean, .default_value = "false", .label = "FULLSCREEN",
     .help = "F11 or Alt+Enter switch it too."},
    {.key = PICCOLO_DISPLAY_FULLSCREEN_TYPE, .type = SettingType::enumeration, .default_value = "borderless",
     .values = FULLSCREEN_TYPE_VALUES, .value_count = FULLSCREEN_TYPE_COUNT, .label = "FULLSCREEN TYPE",
     .help = "Borderless: a window covering the screen (quick to switch). Exclusive: the display is piccolo's, at "
             "its current resolution."},
    {.key = PICCOLO_WINDOW_MODE, .type = SettingType::enumeration, .default_value = "fixed",
     .values = WINDOW_MODE_VALUES, .value_count = WINDOW_MODE_COUNT, .label = "WINDOW MODE",
     .help = "Fixed size: the window keeps its own shape and size. Dynamic: the window is the game's picture, "
             "resized when the game changes size."},
    {.key = PICCOLO_WINDOW_ASPECT, .type = SettingType::enumeration, .default_value = "16:9",
     .values = WINDOW_ASPECT_VALUES, .value_count = WINDOW_ASPECT_COUNT, .label = "WINDOW ASPECT",
     .help = "Fixed size: the window's shape."},
    {.key = PICCOLO_WINDOW_HEIGHT, .type = SettingType::enumeration, .default_value = "1080",
     .values = WINDOW_HEIGHT_VALUES, .value_count = WINDOW_HEIGHT_COUNT, .label = "WINDOW HEIGHT",
     .help = "Fixed size: the window's height. Smaller if the screen has less room."},
    {.key = PICCOLO_WINDOW_SCALE, .type = SettingType::integer, .default_value = "3", .min = 1, .max = 10,
     .label = "WINDOW SCALE", .help = "Dynamic: the window is the game's picture times this. Lowered when the "
                                      "screen has less room."},
    {.key = PICCOLO_WINDOW_REMEMBER, .type = SettingType::boolean, .default_value = "true",
     .label = "REMEMBER WINDOW SIZE AND POSITION",
     .help = "Start where and how big the window was last time. (Some desktops, like Wayland ones, place windows "
             "themselves.)"},
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

uint32_t piccolo_settings_frames_in_flight(const Settings* settings)
{
    return settings_get_enum(settings, PICCOLO_LATENCY_FRAMES_IN_FLIGHT) == 1 ? 1u : 2u;
}

// Frames in flight (enum index), wait for present, frame delay (ms).
struct LatencyPreset
{
    const char* name;
    uint32_t    frames_in_flight_index;
    bool        wait_for_present;
    int32_t     frame_delay;
};

static const LatencyPreset LATENCY_PRESETS[PICCOLO_LATENCY_PRESET_COUNT] = {
    {"OFF", 0, false, 0},
    {"LOW", 1, true, 0},
    {"LOWEST", 1, true, 4},
};

const char* piccolo_settings_latency_preset_name(uint32_t preset)
{
    return preset < PICCOLO_LATENCY_PRESET_COUNT ? LATENCY_PRESETS[preset].name : "CUSTOM";
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
