#include "big_picture.h"

#include <cfloat>
#include <cstdio>
#include <cstring>
#include <string>

#include <SDL3/SDL.h>

#include "imgui.h"

#include "app_shell.h"
#include "debug_ui.h"
#include "input_glyphs.h"
#include "piccolo_text.h"
#include "pixel_font.h"
#include "pixel_font_imgui.h"
#include "retro_ui.h"

// The logo's colors, top rows to bottom (gold to orange) and its shadow.
// Fixed, not the theme's: the logo is the same in every theme.
constexpr uint32_t LOGO_TOP = 0xFFFFE070u;
constexpr uint32_t LOGO_MIDDLE = 0xFFF8B830u;
constexpr uint32_t LOGO_BOTTOM = 0xFFE07818u;
constexpr uint32_t LOGO_SHADOW = 0xFF000000u;
constexpr uint32_t ACCENT = 0xFFF8C838u;      // the core's name under the menu
constexpr uint32_t WINDOW_LIGHT = 0xFFF8C838u;   // lit windows in the skyline

// Logo pixels are this many retro pixels.
constexpr int32_t LOGO_BLOCK = 5;

struct HomeItem
{
    Str              label;
    BigPictureAction action;
    bool             enabled;
};

// LOAD CORE is there to show where it goes: picking a core on its own (and
// "auto" by file type) isn't done yet, LOAD GAME asks for the core.
static const HomeItem HOME_ITEMS[] = {
    {Str::bp_load_game, BigPictureAction::load_game, true},
    {Str::bp_load_core, BigPictureAction::load_core, false},
    {Str::bp_settings, BigPictureAction::settings, true},
    {Str::bp_quit, BigPictureAction::quit, true},
};
constexpr int32_t HOME_ITEM_COUNT = static_cast<int32_t>(sizeof(HOME_ITEMS) / sizeof(HOME_ITEMS[0]));

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

static ImU32 color_u32(uint32_t argb)
{
    return IM_COL32((argb >> 16) & 0xFFu, (argb >> 8) & 0xFFu, argb & 0xFFu, argb >> 24);
}

static ImU32 color_alpha(uint32_t argb, uint32_t alpha)
{
    return IM_COL32((argb >> 16) & 0xFFu, (argb >> 8) & 0xFFu, argb & 0xFFu, alpha);
}

// `a` moved toward `b` by amount/256.
static uint32_t mix(uint32_t a, uint32_t b, uint32_t amount)
{
    uint32_t result = 0xFF000000u;
    for (int shift = 0; shift <= 16; shift += 8)
    {
        uint32_t ca = (a >> shift) & 0xFFu;
        uint32_t cb = (b >> shift) & 0xFFu;
        uint32_t channel = (ca * (256u - amount) + cb * amount) / 256u;
        result |= channel << shift;
    }
    return result;
}

static void rect(ImDrawList* draw_list, float x, float y, float w, float h, ImU32 color)
{
    draw_list->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), color);
}

// A small fixed random sequence (the same sky every frame and every run).
static uint32_t next_random(uint32_t* state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state >> 8;
}

static float text_width(const char* text, float block)
{
    float size = PIXEL_FONT_IMGUI_UNIT * block;
    return ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x;
}

static void text(ImDrawList* draw_list, float x, float y, const char* label, uint32_t color, float block)
{
    draw_list->AddText(ImGui::GetFont(), PIXEL_FONT_IMGUI_UNIT * block, ImVec2(x, y), color_u32(color), label);
}

// A box: face, a raised edge (light top/left, dark bottom/right) and a dark
// outline, one retro pixel each.
static void panel(ImDrawList* draw_list, float x, float y, float w, float h, float block, const RetroUiPalette* palette)
{
    rect(draw_list, x - block, y - block, w + 2 * block, h + 2 * block, color_u32(palette->text_shadow));
    rect(draw_list, x, y, w, h, color_u32(palette->panel_face));
    rect(draw_list, x, y, w, block, color_u32(palette->panel_light));
    rect(draw_list, x, y, block, h, color_u32(palette->panel_light));
    rect(draw_list, x, y + h - block, w, block, color_u32(palette->panel_dark));
    rect(draw_list, x + w - block, y, block, h, color_u32(palette->panel_dark));
}

// ---------------------------------------------------------------------------
// Background: sky, stars, skyline
// ---------------------------------------------------------------------------

static void draw_background(ImDrawList* draw_list, float width, float height, float block, const RetroUiPalette* palette)
{
    // The sky: the theme's darkest blue at the top to its field color at the bottom.
    uint32_t top = palette->panel_dark;
    uint32_t bottom = palette->field_face;
    draw_list->AddRectFilledMultiColor(ImVec2(0.0f, 0.0f), ImVec2(width, height), color_u32(top), color_u32(top),
                                       color_u32(bottom), color_u32(bottom));

    // Stars: one retro pixel each, a few brighter "plus" ones; some twinkle.
    int32_t columns = static_cast<int32_t>(width / block);
    int32_t rows = static_cast<int32_t>(height / block);
    if (columns <= 0 || rows <= 0)
    {
        return;
    }
    uint32_t random = 12345u;
    int32_t star_count = columns * rows / 700;
    double time = ImGui::GetTime();
    for (int32_t i = 0; i < star_count; i++)
    {
        float x = static_cast<float>(next_random(&random) % static_cast<uint32_t>(columns)) * block;
        float y = static_cast<float>(next_random(&random) % static_cast<uint32_t>(rows * 3 / 4)) * block;
        uint32_t kind = next_random(&random) % 16u;
        uint32_t phase = next_random(&random) % 1000u;
        // A twinkling star is dim for a moment every few seconds.
        bool dim = kind < 4 && static_cast<uint32_t>(time * 400.0 + phase * 7) % 3000u < 400u;
        uint32_t alpha = dim ? 60u : (kind < 12 ? 140u : 255u);
        ImU32 color = color_alpha(palette->text, alpha);
        rect(draw_list, x, y, block, block, color);
        if (kind == 15 && !dim)
        {
            ImU32 glow = color_alpha(palette->text, 90u);
            rect(draw_list, x - block, y, block, block, glow);
            rect(draw_list, x + block, y, block, block, glow);
            rect(draw_list, x, y - block, block, block, glow);
            rect(draw_list, x, y + block, block, block, glow);
        }
    }

    // The skyline: two rows of buildings, the far one lighter, with a few lit windows.
    for (int layer = 0; layer < 2; layer++)
    {
        uint32_t face = layer == 0 ? mix(bottom, palette->text, 24) : mix(top, bottom, 96);
        int32_t lowest = layer == 0 ? 16 : 6;
        int32_t spread = layer == 0 ? 40 : 26;
        uint32_t layer_random = layer == 0 ? 777u : 4242u;
        float x = -static_cast<float>(next_random(&layer_random) % 8u) * block;
        while (x < width)
        {
            float w = static_cast<float>(6 + next_random(&layer_random) % 14u) * block;
            float h = static_cast<float>(lowest + static_cast<int32_t>(next_random(&layer_random) % static_cast<uint32_t>(spread))) * block;
            rect(draw_list, x, height - h, w, h, color_u32(face));
            // Windows: a grid every 3 retro pixels, some of them lit.
            for (float wy = height - h + 2 * block; wy < height - 2 * block; wy += 3 * block)
            {
                for (float wx = x + 2 * block; wx < x + w - 2 * block; wx += 3 * block)
                {
                    if (next_random(&layer_random) % 23u == 0u)
                    {
                        rect(draw_list, wx, wy, block, block, color_alpha(WINDOW_LIGHT, layer == 0 ? 70u : 150u));
                    }
                }
            }
            x += w;
        }
    }
}

// ---------------------------------------------------------------------------
// Logo
// ---------------------------------------------------------------------------

// "PICCOLO" from the pixel font, each font pixel a LOGO_BLOCK square, colored
// by row (gold to orange) over a hard shadow. Top centered at `y`.
static float draw_logo(ImDrawList* draw_list, float center_x, float y, float block)
{
    const char* word = "PICCOLO";
    float pixel = LOGO_BLOCK * block;
    int32_t letters = 0;
    for (const char* c = word; *c != 0; c++)
    {
        letters++;
    }
    float width = static_cast<float>(letters * PIXEL_FONT_ADVANCE - 1) * pixel;
    float left = static_cast<float>(static_cast<int32_t>(center_x - width / 2.0f));
    float shadow = 2.0f * block;
    // The letter itself is rows 2..6 of the cell: start the logo at row 2.
    float top = y - PIXEL_FONT_MARK_ROWS * pixel;

    for (int pass = 0; pass < 2; pass++)
    {
        float x = left;
        for (const char* c = word; *c != 0; c++)
        {
            uint8_t rows[PIXEL_FONT_CELL_HEIGHT] = {};
            pixel_font_glyph_rows(static_cast<uint32_t>(*c), rows);
            for (int row = 0; row < PIXEL_FONT_CELL_HEIGHT; row++)
            {
                uint32_t color = row <= 3 ? LOGO_TOP : (row <= 4 ? LOGO_MIDDLE : LOGO_BOTTOM);
                for (int column = 0; column < PIXEL_FONT_GLYPH_WIDTH; column++)
                {
                    if ((rows[row] & (1u << (PIXEL_FONT_GLYPH_WIDTH - 1 - column))) == 0)
                    {
                        continue;
                    }
                    float px = x + static_cast<float>(column) * pixel;
                    float py = top + static_cast<float>(row) * pixel;
                    if (pass == 0)
                    {
                        rect(draw_list, px + shadow, py + shadow, pixel, pixel, color_u32(LOGO_SHADOW));
                    }
                    else
                    {
                        rect(draw_list, px, py, pixel, pixel, color_u32(color));
                        // A one-retro-pixel highlight on the top edge of each block.
                        rect(draw_list, px, py, pixel, block, color_alpha(0xFFFFFFFFu, 70u));
                    }
                }
            }
            x += PIXEL_FONT_ADVANCE * pixel;
        }
    }
    return PIXEL_FONT_GLYPH_HEIGHT * pixel + shadow;
}

// The branding logo (icon + wordmark, smooth art from assets/branding):
// loaded once from next to the program. Without it, the pixel logo above.
static void load_logo(BigPictureHome* home)
{
    home->logo_tried = true;
    const char* base = SDL_GetBasePath();   // ends with a separator; SDL owns it
    std::string path = std::string(base != nullptr ? base : "") + "assets/branding/png/piccolo-logo-dark.png";
    SDL_Surface* png = SDL_LoadPNG(path.c_str());
    SDL_Surface* converted = png != nullptr ? SDL_ConvertSurface(png, SDL_PIXELFORMAT_ARGB8888) : nullptr;
    if (converted != nullptr && converted->w > 0 && converted->h > 0 && converted->w <= 4096 && converted->h <= 4096)
    {
        home->logo_width = static_cast<uint32_t>(converted->w);
        home->logo_height = static_cast<uint32_t>(converted->h);
        home->logo_pixels.resize(static_cast<size_t>(home->logo_width) * home->logo_height);
        for (uint32_t y = 0; y < home->logo_height; y++)
        {
            const uint8_t* row = static_cast<const uint8_t*>(converted->pixels) + static_cast<size_t>(y) * static_cast<size_t>(converted->pitch);
            std::memcpy(&home->logo_pixels[static_cast<size_t>(y) * home->logo_width], row,
                        static_cast<size_t>(home->logo_width) * 4);
        }
        home->logo_upload_pending = true;
    }
    else
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "HOME: no logo at %s (%s); using the pixel one", path.c_str(), SDL_GetError());
    }
    SDL_DestroySurface(converted);
    SDL_DestroySurface(png);
}

void big_picture_home_upload(BigPictureHome* home, AppShell* shell)
{
    if (!home->logo_upload_pending)
    {
        return;
    }
    if (home->logo_texture.image == VK_NULL_HANDLE)
    {
        if (!streaming_texture_create(&home->logo_texture, &shell->vulkan, home->logo_width, home->logo_height))
        {
            home->logo_upload_pending = false;
            return;
        }
        home->logo_texture_id = debug_ui_add_texture(home->logo_texture.view);
    }
    if (!app_shell_acquire(shell))
    {
        return;   // try on the next frame
    }
    if (streaming_texture_upload(&home->logo_texture, renderer_command_buffer(&shell->renderer),
                                 renderer_frame_index(&shell->renderer), home->logo_pixels.data(), home->logo_width,
                                 home->logo_height))
    {
        home->logo_ready = true;
        home->logo_pixels.clear();
        home->logo_pixels.shrink_to_fit();   // on the GPU now
    }
    home->logo_upload_pending = false;
}

void big_picture_home_destroy(BigPictureHome* home, AppShell* shell)
{
    if (home->logo_texture.image != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(shell->vulkan.device);
        if (home->logo_texture_id != 0)
        {
            debug_ui_remove_texture(home->logo_texture_id);
        }
        streaming_texture_destroy(&home->logo_texture, &shell->vulkan);
    }
    home->logo_texture_id = 0;
    home->logo_ready = false;
}

// The branding logo, LOGO_HEIGHT retro pixels tall, top centered at `y`;
// smooth art, so drawn with linear filtering. Returns its height.
constexpr float LOGO_HEIGHT = 40.0f;

static float draw_branding_logo(const BigPictureHome* home, ImDrawList* draw_list, float center_x, float y, float block)
{
    float height = LOGO_HEIGHT * block;
    float width = height * static_cast<float>(home->logo_width) / static_cast<float>(home->logo_height);
    float left = static_cast<float>(static_cast<int32_t>(center_x - width / 2.0f));
    draw_list->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear, nullptr);
    draw_list->AddImage(ImTextureRef(static_cast<ImTextureID>(home->logo_texture_id)), ImVec2(left, y),
                        ImVec2(left + width, y + height));
    draw_list->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest, nullptr);
    return height;
}

// ---------------------------------------------------------------------------
// Hints bar
// ---------------------------------------------------------------------------

struct Hint
{
    GlyphAction action;
    Str         label;
};

// The bar at the bottom: the version on the left, button hints on the right.
static void draw_hints_bar(ImDrawList* draw_list, float width, float height, float block, const BigPictureContext* context,
                           const Hint* hints, int32_t hint_count)
{
    const RetroUiPalette* palette = context->palette;
    float glyph = input_glyphs_size(context->glyphs);
    float bar_h = 14.0f * block;   // a text line and padding; glyphs are about text height
    if (bar_h < glyph + 4.0f * block)
    {
        bar_h = glyph + 4.0f * block;
    }
    float margin = 8.0f * block;
    float bar_x = margin;
    float bar_y = height - margin - bar_h;
    float bar_w = width - 2.0f * margin;
    panel(draw_list, bar_x, bar_y, bar_w, bar_h, block, palette);

    // Text is 5 pixels tall at rows 2..6 of its 8-pixel cell: centered on the bar.
    float text_y = static_cast<float>(static_cast<int32_t>(bar_y + bar_h / 2.0f - 4.5f * block));
    char version[32] = {};
    std::snprintf(version, sizeof(version), "V%s", PICCOLO_VERSION);
    text(draw_list, bar_x + 5.0f * block, text_y, version, palette->text_disabled, block);

    // Right to left, so the last hint ends at the bar's right padding.
    float glyph_gap = block;
    float glyph_y = static_cast<float>(static_cast<int32_t>(bar_y + (bar_h - glyph) / 2.0f));
    float right = bar_x + bar_w - 5.0f * block;
    for (int32_t i = hint_count - 1; i >= 0; i--)
    {
        const char* label = piccolo_text(context->strings, hints[i].label);
        float label_w = text_width(label, block);
        right -= label_w;
        text(draw_list, right, text_y, label, palette->text, block);
        float glyph_w = input_glyphs_width(context->glyphs, hints[i].action, glyph_gap);
        if (glyph_w > 0.0f)
        {
            right -= 3.0f * block + glyph_w;
            input_glyphs_draw(context->glyphs, draw_list, hints[i].action, right, glyph_y, glyph, glyph_gap, palette->text);
        }
        right -= 10.0f * block;
    }
}

// ---------------------------------------------------------------------------
// HOME
// ---------------------------------------------------------------------------

static bool pressed(ImGuiKey key)
{
    return ImGui::IsKeyPressed(key, true);
}

// The next enabled item from `from` going `step` (+1 / -1), wrapping around.
static int32_t step_selection(int32_t from, int32_t step)
{
    int32_t index = from;
    for (int32_t i = 0; i < HOME_ITEM_COUNT; i++)
    {
        index = (index + step + HOME_ITEM_COUNT) % HOME_ITEM_COUNT;
        if (HOME_ITEMS[index].enabled)
        {
            return index;
        }
    }
    return from;
}

BigPictureAction big_picture_home_draw(BigPictureHome* home, const BigPictureContext* context)
{
    const RetroUiPalette* palette = context->palette;
    float block = static_cast<float>(context->scale);
    const ImGuiIO& io = ImGui::GetIO();
    float width = io.DisplaySize.x;
    float height = io.DisplaySize.y;
    if (home->selected < 0 || home->selected >= HOME_ITEM_COUNT || !HOME_ITEMS[home->selected].enabled)
    {
        home->selected = 0;
    }

    // One window over the whole screen, kept behind every other window
    // (LOAD GAME and SETTINGS open on top of it). No ImGui navigation: the
    // keys and buttons are read below, so focus never gets lost.
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##big_picture_home", nullptr, flags);
    ImGui::PopStyleVar(2);
    ImGui::PushFont(context->pixel_font, PIXEL_FONT_IMGUI_UNIT * block);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest, nullptr);

    draw_background(draw_list, width, height, block, palette);

    // Logo and subtitle, then the menu, then what's loaded: one column,
    // centered, a bit above the middle (the hints bar takes the bottom).
    float row_h = 13.0f * block;
    if (!home->logo_tried)
    {
        load_logo(home);
    }
    bool branding = home->logo_ready && home->logo_texture_id != 0;
    float logo_h = branding ? LOGO_HEIGHT * block : PIXEL_FONT_GLYPH_HEIGHT * LOGO_BLOCK * block + 2.0f * block;
    float column_h = logo_h + 8.0f * block + 8.0f * block + 12.0f * block + row_h * HOME_ITEM_COUNT + 8.0f * block +
                     14.0f * block + 20.0f * block;
    float center_x = width / 2.0f;
    float y = static_cast<float>(static_cast<int32_t>((height - column_h) / 2.0f - 6.0f * block));
    if (y < 8.0f * block)
    {
        y = 8.0f * block;
    }
    y += (branding ? draw_branding_logo(home, draw_list, center_x, y, block) : draw_logo(draw_list, center_x, y, block)) +
         8.0f * block;

    const char* subtitle = piccolo_text(context->strings, Str::bp_subtitle);
    float subtitle_w = text_width(subtitle, block);
    text(draw_list, static_cast<float>(static_cast<int32_t>(center_x - subtitle_w / 2.0f)), y, subtitle,
         palette->text_disabled, block);
    y += 8.0f * block + 12.0f * block;

    // The menu box: as wide as the longest item plus room for the arrow.
    float label_w = 0.0f;
    for (int32_t i = 0; i < HOME_ITEM_COUNT; i++)
    {
        float w = text_width(piccolo_text(context->strings, HOME_ITEMS[i].label), block);
        label_w = w > label_w ? w : label_w;
    }
    float pad = 3.0f * block;
    float box_w = label_w + 26.0f * block;
    if (box_w < 120.0f * block)
    {
        box_w = 120.0f * block;
    }
    float box_h = row_h * HOME_ITEM_COUNT + 2.0f * pad;
    float box_x = static_cast<float>(static_cast<int32_t>(center_x - box_w / 2.0f));
    float box_y = y;
    panel(draw_list, box_x, box_y, box_w, box_h, block, palette);

    BigPictureAction picked = BigPictureAction::none;
    bool mouse_moved = io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f;
    for (int32_t i = 0; i < HOME_ITEM_COUNT; i++)
    {
        const HomeItem* item = &HOME_ITEMS[i];
        float row_x = box_x + pad;
        float row_y = box_y + pad + row_h * static_cast<float>(i);
        float row_w = box_w - 2.0f * pad;

        // The mouse: hovering selects, clicking picks.
        ImGui::SetCursorScreenPos(ImVec2(row_x, row_y));
        ImGui::PushID(i);
        bool clicked = ImGui::InvisibleButton("##item", ImVec2(row_w, row_h));
        ImGui::PopID();
        if (item->enabled && context->takes_input && ImGui::IsItemHovered() && mouse_moved)
        {
            home->selected = i;
        }
        if (item->enabled && context->takes_input && clicked)
        {
            home->selected = i;
            picked = item->action;
        }

        bool lit = i == home->selected;
        uint32_t color = !item->enabled ? palette->text_disabled : (lit ? palette->text_highlight : palette->text);
        float label_y = row_y + (row_h - 8.0f * block) / 2.0f;
        if (lit)
        {
            rect(draw_list, row_x, row_y, row_w, row_h, color_u32(palette->highlight));
            text(draw_list, row_x + 4.0f * block, label_y, PIXEL_FONT_UTF8_ARROW_RIGHT, color, block);
        }
        text(draw_list, row_x + 14.0f * block, label_y, piccolo_text(context->strings, item->label), color, block);
    }
    y = box_y + box_h + 12.0f * block;

    // What's loaded: the last core, and no game (HOME only shows without one).
    // "CORE" and "· LAST USED" dim, the core's name in the accent color.
    const char* core_word = piccolo_text(context->strings, Str::bp_core);
    const char* core_name = context->core_name;
    char tail[128] = {};
    if (core_name[0] != 0)
    {
        std::snprintf(tail, sizeof(tail), " \xC2\xB7 %s", piccolo_text(context->strings, Str::bp_last_used));
    }
    else
    {
        core_name = piccolo_text(context->strings, Str::bp_core_none);
    }
    float space = text_width(" ", block);
    float word_w = text_width(core_word, block);
    float name_w = text_width(core_name, block);
    float line_w = word_w + space + name_w + text_width(tail, block);
    float line_x = static_cast<float>(static_cast<int32_t>(center_x - line_w / 2.0f));
    text(draw_list, line_x, y, core_word, palette->text_disabled, block);
    text(draw_list, line_x + word_w + space, y, core_name, ACCENT, block);
    text(draw_list, line_x + word_w + space + name_w, y, tail, palette->text_disabled, block);
    y += 10.0f * block;
    const char* no_game = piccolo_text(context->strings, Str::bp_no_game);
    text(draw_list, static_cast<float>(static_cast<int32_t>(center_x - text_width(no_game, block) / 2.0f)), y, no_game,
         palette->text_disabled, block);

    static const Hint HINTS[] = {
        {GlyphAction::move_vertical, Str::bp_hint_move},
        {GlyphAction::confirm, Str::bp_hint_select},
    };
    draw_hints_bar(draw_list, width, height, block, context, HINTS, 2);

    // Keys and buttons: up/down move (with repeat), confirm picks.
    if (context->takes_input)
    {
        if (pressed(ImGuiKey_UpArrow) || pressed(ImGuiKey_GamepadDpadUp) || pressed(ImGuiKey_GamepadLStickUp))
        {
            home->selected = step_selection(home->selected, -1);
        }
        if (pressed(ImGuiKey_DownArrow) || pressed(ImGuiKey_GamepadDpadDown) || pressed(ImGuiKey_GamepadLStickDown))
        {
            home->selected = step_selection(home->selected, 1);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false))
        {
            picked = HOME_ITEMS[home->selected].action;
        }
    }

    draw_list->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear, nullptr);
    ImGui::PopFont();
    ImGui::End();
    return picked;
}
