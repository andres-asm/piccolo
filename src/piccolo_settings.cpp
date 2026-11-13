#include "piccolo_settings.h"

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
