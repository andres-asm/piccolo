#include "retro_ui.h"

#include "imgui.h"
#include "imgui_internal.h"   // DC.CurrLineTextBaseOffset: where ImGui puts a widget's text on its line

#include "piccolo_text.h"
#include "pixel_font_imgui.h"

// The scroll bar's width and the gap before it, in retro pixels.
constexpr float RETRO_UI_SCROLLBAR_WIDTH = 5.0f;
constexpr float RETRO_UI_SCROLLBAR_GAP = 4.0f;

// The built-in themes (0xAARRGGBB).
static const RetroUiPalette THEMES[] = {
    // PICCOLO: the original colors, no drop shadows on windows.
    {0xFF000000u, false,
     0xFF34507Cu, 0xFF34507Cu, 0xFF5878A8u, 0xFF1A2840u,
     false, 0xFF34507Cu, 0xFF5878A8u, 0xFF5878A8u,
     0xFF2A4068u, 0xFF2A4068u, 0xFF5878A8u, 0xFF101828u,
     0xFF1E2E4Cu, 0xFF26385Cu, 0xFF7090C8u,
     0xFFE0E6F0u, 0xFFFFFFFFu, 0xFF8090B0u, 0xFF101828u,
     0xFF34507Cu, 0xFF34507Cu,
     0xFFFFFFFFu, 0xFFE0E6F0u,
     0xFF34507Cu, 0xFF34507Cu, 0xFF34507Cu, 0xFFE0E6F0u, 0xFFE0E6F0u, false, 0xFFF07030u, 0xFF34507Cu},
    // CLASSIC: purple background, a deep blue gradient bar with grey boxes
    // behind the menu titles, grey menus with a magenta highlight, blue
    // windows with black fields, drop shadows everywhere.
    {0xFF422D84u, true,
     0xFF002D8Cu, 0xFF0049FFu, 0xFF0049FFu, 0xFF001A5Cu,
     true, 0xFF5B5552u, 0xFF7B7573u, 0xFF3A3531u,
     0xFF5B5D5Bu, 0xFF2120ACu, 0xFF3439BAu, 0xFF00008Cu,
     0xFF000000u, 0xFF101060u, 0xFFA50052u,
     0xFFCECFCEu, 0xFFFFFFFFu, 0xFF8C8682u, 0xFF292521u,
     0xFF7C76A6u, 0xFF383C5Fu,
     0xFFB01818u, 0xFF9C9C68u,
     0xFF7C76A6u, 0xFFA29EC8u, 0xFF262840u, 0xFFCECFCEu, 0xFF00E000u, true, 0xFFF07030u, 0xFF383C5Fu},
    // MODERN: flat and dark, never a shadow; the menus are the bar's color.
    {0xFF000000u, false,
     0xFF1E222Au, 0xFF1E222Au, 0xFF1E222Au, 0xFF1E222Au,
     false, 0xFF1E222Au, 0xFF1E222Au, 0xFF1E222Au,
     0xFF1E222Au, 0xFF262B34u, 0xFF333944u, 0xFF15181Du,
     0xFF181B21u, 0xFF222630u, 0xFF3D7BD9u,
     0xFFE6E9EFu, 0xFFFFFFFFu, 0xFF7D8494u, 0xFF0F1115u,
     0xFF1E222Au, 0xFF1E222Au,
     0xFFFFFFFFu, 0xFFE6E9EFu,
     0xFF2E3440u, 0xFF2E3440u, 0xFF2E3440u, 0xFFE6E9EFu, 0xFFE6E9EFu, false, 0xFFFFFFFFu, 0xFF2E3440u},
};

static RetroUiPalette g_palette = THEMES[0];

const RetroUiPalette* retro_ui_theme_palette(RetroUiTheme theme)
{
    uint32_t index = static_cast<uint32_t>(theme);
    return &THEMES[index < static_cast<uint32_t>(RetroUiTheme::count) ? index : 0];
}

void retro_ui_set_palette(const RetroUiPalette* palette)
{
    g_palette = *palette;
    // Lines drawn from the font texture would look like text to
    // retro_ui_text_shadows(); without it they are plain shapes (and the
    // UI's lines are one pixel, square, anyway).
    ImGui::GetStyle().AntiAliasedLinesUseTex = false;
}

static ImVec4 to_imgui(uint32_t argb)
{
    return ImVec4(static_cast<float>((argb >> 16) & 0xFFu) / 255.0f, static_cast<float>((argb >> 8) & 0xFFu) / 255.0f,
                  static_cast<float>(argb & 0xFFu) / 255.0f, static_cast<float>(argb >> 24) / 255.0f);
}

static ImU32 to_imgui_u32(uint32_t argb)
{
    return ImGui::ColorConvertFloat4ToU32(to_imgui(argb));
}

// ImGui's borders are one flat color, so the raised edge is drawn by hand:
// light top/left, dark bottom/right, one retro pixel (`block` window pixels) thick.
static void draw_bevel(ImVec2 position, ImVec2 size, float block, uint32_t light, uint32_t dark, bool sides)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 end = ImVec2(position.x + size.x, position.y + size.y);
    draw_list->AddRectFilled(position, ImVec2(end.x, position.y + block), to_imgui_u32(light));
    draw_list->AddRectFilled(ImVec2(position.x, end.y - block), end, to_imgui_u32(dark));
    if (sides)
    {
        draw_list->AddRectFilled(position, ImVec2(position.x + block, end.y), to_imgui_u32(light));
        draw_list->AddRectFilled(ImVec2(end.x - block, position.y), end, to_imgui_u32(dark));
    }
}

// With the palette's shadows: a drop shadow to the right of and below the
// current window (2 retro pixels, the text shadow color at 25% opacity),
// drawn in its own draw list with the clip opened up past its edges.
static void draw_window_shadow(float block)
{
    if (!g_palette.shadows)
    {
        return;
    }
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImVec2 min = window->Pos;
    ImVec2 max(min.x + window->Size.x, min.y + window->Size.y);
    float offset = 2.0f * block;
    ImU32 color = (to_imgui_u32(g_palette.text_shadow) & ~IM_COL32_A_MASK) | (64u << IM_COL32_A_SHIFT);
    ImDrawList* draw_list = window->DrawList;
    draw_list->PushClipRect(min, ImVec2(max.x + offset, max.y + offset), false);
    draw_list->AddRectFilled(ImVec2(max.x, min.y + offset), ImVec2(max.x + offset, max.y + offset), color);
    draw_list->AddRectFilled(ImVec2(min.x + offset, max.y), ImVec2(max.x, max.y + offset), color);
    draw_list->PopClipRect();
}

// Text from the pixel font must be sampled with nearest filtering to stay sharp.
// The callbacks switch the sampler for the rest of this window's draw list.
static void use_nearest_sampler()
{
    ImGui::GetWindowDrawList()->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest, nullptr);
}

static void use_linear_sampler()
{
    ImGui::GetWindowDrawList()->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear, nullptr);
}

// ImGui has no shadowed text, so menu labels are drawn by us: the widgets get
// a fully transparent text color (they still handle layout, hover, clicks and
// keyboard navigation, and draw their highlight), and we paint shadow + text
// on top at the exact position ImGui would have used.
static void draw_label(ImDrawList* draw_list, ImVec2 text_position, const char* label, bool lit)
{
    ImFont* font = ImGui::GetFont();
    float size = ImGui::GetFontSize();
    draw_list->AddText(font, size, text_position, to_imgui_u32(lit ? g_palette.text_highlight : g_palette.text), label);
}

// Where ImGui will put the text of the next widget on this line.
static ImVec2 next_text_position(float extra_x)
{
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    return ImVec2(cursor.x + extra_x, cursor.y + ImGui::GetCurrentWindow()->DC.CurrLineTextBaseOffset);
}

static void push_invisible_text()
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
}

int32_t retro_ui_scale_for_width(float window_width)
{
    int32_t scale = static_cast<int32_t>(window_width / 640.0f);
    return scale < 1 ? 1 : scale;
}

// With menu boxes the box is the title's highlight: ImGui's own (wider than
// the box, it showed around it) is made invisible for the bar's titles. The
// dropdowns' items keep theirs (BeginMenu has returned before they're drawn).
static void push_bar_highlight()
{
    ImVec4 none(0.0f, 0.0f, 0.0f, 0.0f);
    ImVec4 header = g_palette.menu_boxes ? none : to_imgui(g_palette.highlight);
    ImGui::PushStyleColor(ImGuiCol_Header, header);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, header);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, header);
}

static void pop_bar_highlight()
{
    ImGui::PopStyleColor(3);
}

// With the palette's menu_boxes: a raised box behind a menu title on the bar
// (lit = the highlight color), from just before the label to just after it,
// one retro pixel short of the bar's top and bottom. The bar's top and bottom
// come from the caller: once a menu opens, the current window is its dropdown.
static void draw_title_box(ImDrawList* draw_list, ImVec2 label_position, const char* label, bool lit, float block,
                           float bar_top, float bar_bottom)
{
    if (!g_palette.menu_boxes)
    {
        return;
    }
    float text_width = ImGui::CalcTextSize(label).x;
    ImVec2 min(label_position.x - 3.0f * block, bar_top + block);
    ImVec2 max(label_position.x + text_width + 3.0f * block, bar_bottom - block);
    draw_list->AddRectFilled(min, max, to_imgui_u32(lit ? g_palette.highlight : g_palette.menu_box_face));
    draw_list->AddRectFilled(min, ImVec2(max.x, min.y + block), to_imgui_u32(g_palette.menu_box_light));
    draw_list->AddRectFilled(min, ImVec2(min.x + block, max.y), to_imgui_u32(g_palette.menu_box_light));
    draw_list->AddRectFilled(ImVec2(min.x, max.y - block), max, to_imgui_u32(g_palette.menu_box_dark));
    draw_list->AddRectFilled(ImVec2(max.x - block, min.y), max, to_imgui_u32(g_palette.menu_box_dark));
}

// The bar's background: face at the top and bottom, center in the middle
// (the same color twice = flat).
static void draw_bar_gradient(float block)
{
    ImGuiWindow* bar = ImGui::GetCurrentWindow();
    ImDrawList* draw_list = bar->DrawList;
    ImVec2 top = bar->Pos;
    float middle = top.y + static_cast<float>(static_cast<int32_t>(bar->Size.y * 0.5f / block)) * block;
    ImVec2 end(top.x + bar->Size.x, top.y + bar->Size.y);
    ImU32 face = to_imgui_u32(g_palette.bar_face);
    ImU32 center = to_imgui_u32(g_palette.bar_center);
    draw_list->AddRectFilledMultiColor(top, ImVec2(end.x, middle), face, face, center, center);
    draw_list->AddRectFilledMultiColor(ImVec2(top.x, middle), end, center, center, face, face);
}

// One dropdown item drawn our way (invisible ImGui text, our shadowed label).
static bool menu_item(const char* label)
{
    ImVec2 item_position = next_text_position(0.0f);
    push_invisible_text();
    bool clicked = ImGui::MenuItem(label);
    ImGui::PopStyleColor();
    draw_label(ImGui::GetWindowDrawList(), item_position, label, ImGui::IsItemHovered());
    return clicked;
}

// The RECENT menu: first on the bar, a hamburger glyph as its label.
static bool recent_menu(const Localization* strings, const char* const* recent, int32_t recent_count,
                        int32_t* recent_picked, float block)
{
    ImDrawList* bar_draw_list = ImGui::GetWindowDrawList();
    ImGuiWindow* bar = ImGui::GetCurrentWindow();
    float bar_top = bar->Pos.y;
    float bar_bottom = bar->Pos.y + bar->Size.y;
    ImVec2 label_position = next_text_position(IM_TRUNC(ImGui::GetStyle().ItemSpacing.x * 0.5f));
    push_invisible_text();
    push_bar_highlight();
    bool open = ImGui::BeginMenu(PIXEL_FONT_UTF8_MENU);
    pop_bar_highlight();
    ImGui::PopStyleColor();
    bool lit = open || ImGui::IsItemHovered();
    draw_title_box(bar_draw_list, label_position, PIXEL_FONT_UTF8_MENU, lit, block, bar_top, bar_bottom);
    draw_label(bar_draw_list, label_position, PIXEL_FONT_UTF8_MENU, lit);
    if (!open)
    {
        return false;
    }
    use_nearest_sampler();
    draw_window_shadow(block);
    draw_bevel(ImGui::GetWindowPos(), ImGui::GetWindowSize(), block, g_palette.menu_box_light, g_palette.menu_box_dark, true);
    bool picked = false;
    if (recent_count == 0)
    {
        ImGui::BeginDisabled();
        menu_item(piccolo_text(strings, Str::menu_recent_empty));
        ImGui::EndDisabled();
    }
    for (int32_t i = 0; i < recent_count; i++)
    {
        ImGui::PushID(i);
        if (menu_item(recent[i]))
        {
            *recent_picked = i;
            picked = true;
        }
        ImGui::PopID();
    }
    use_linear_sampler();
    ImGui::EndMenu();
    return picked;
}

MenuAction retro_ui_menu_bar(ImFont* pixel_font, int32_t scale, const Localization* strings,
                             const char* const* recent, int32_t recent_count, int32_t* recent_picked)
{
    MenuAction picked = MenuAction::none;
    float block = static_cast<float>(scale);
    *recent_picked = -1;

    // Metrics in retro pixels x scale:
    //   bar  = 2 + 8 (font line) + 2 = 12, label padding 3,
    //   item = 8 (font line) + 2 spacing = 10, panel padding 2 (+4 text indent).
    ImGui::PushFont(pixel_font, PIXEL_FONT_IMGUI_UNIT * block);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f * block, 2.0f * block));
    // 4 retro pixels between menu titles (Andrés: 2 was too tight, 8 too loose).
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * block, 2.0f * block));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f * block, 2.0f * block));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, to_imgui(g_palette.bar_face));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, to_imgui(g_palette.menu_face));
    ImGui::PushStyleColor(ImGuiCol_Text, to_imgui(g_palette.text));
    ImGui::PushStyleColor(ImGuiCol_Header, to_imgui(g_palette.highlight));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, to_imgui(g_palette.highlight));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, to_imgui(g_palette.highlight));
    ImGui::PushStyleColor(ImGuiCol_Border, to_imgui(g_palette.panel_dark));

    if (ImGui::BeginMainMenuBar())
    {
        draw_bar_gradient(block);
        use_nearest_sampler();
        draw_bevel(ImGui::GetWindowPos(), ImGui::GetWindowSize(), block, g_palette.bar_light, g_palette.bar_dark, false);

        if (recent_menu(strings, recent, recent_count, recent_picked, block))
        {
            picked = MenuAction::recent_game;
        }

        ImDrawList* bar_draw_list = ImGui::GetWindowDrawList();
        float bar_top = ImGui::GetWindowPos().y;
        float bar_bottom = bar_top + ImGui::GetWindowSize().y;
        for (int32_t menu = 0; menu < menus_count(); menu++)
        {
            const char* label = piccolo_text(strings, menus_label(menu));

            // On the bar, BeginMenu indents its text by half the item spacing (imgui_widgets.cpp).
            ImVec2 label_position = next_text_position(IM_TRUNC(ImGui::GetStyle().ItemSpacing.x * 0.5f));
            push_invisible_text();
            push_bar_highlight();
            bool open = ImGui::BeginMenu(label);
            pop_bar_highlight();
            ImGui::PopStyleColor();

            // When open, the current window is now the dropdown: the label still
            // goes into the bar's draw list (saved above). Hover is only known
            // when closed; an open menu is lit anyway.
            bool lit = open || ImGui::IsItemHovered();
            draw_title_box(bar_draw_list, label_position, label, lit, block, bar_top, bar_bottom);
            draw_label(bar_draw_list, label_position, label, lit);

            if (open)
            {
                use_nearest_sampler();
                draw_window_shadow(block);
                draw_bevel(ImGui::GetWindowPos(), ImGui::GetWindowSize(), block, g_palette.menu_box_light,
                           g_palette.menu_box_dark, true);
                for (int32_t item = 0; item < menus_item_count(menu); item++)
                {
                    if (menus_item_action(menu, item) == MenuAction::separator)
                    {
                        ImGui::Separator();
                        continue;
                    }
                    if (menu_item(piccolo_text(strings, menus_item_label(menu, item))))
                    {
                        picked = menus_item_action(menu, item);
                    }
                }
                use_linear_sampler();
                ImGui::EndMenu();
            }
        }

        use_linear_sampler();
        ImGui::EndMainMenuBar();
    }

    ImGui::PopStyleColor(7);
    ImGui::PopStyleVar(8);
    ImGui::PopFont();
    return picked;
}

void retro_ui_status_line(ImFont* pixel_font, int32_t scale, const char* text)
{
    float block = static_cast<float>(scale);
    const ImGuiIO& io = ImGui::GetIO();

    // A transparent, input-less window over the whole screen, kept behind every
    // other window, just to have somewhere to draw.
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    ImGui::Begin("##retro_status", nullptr, flags);
    ImGui::PushFont(pixel_font, PIXEL_FONT_IMGUI_UNIT * block);
    use_nearest_sampler();
    // 4 retro pixels from the left and from the bottom (line = 7, glyph starts 1 down).
    ImVec2 position = ImVec2(4.0f * block, io.DisplaySize.y - (4.0f + 6.0f) * block);
    draw_label(ImGui::GetWindowDrawList(), position, text, false);
    use_linear_sampler();
    ImGui::PopFont();
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Theme for windows and widgets
// ---------------------------------------------------------------------------


// A selected list entry: halfway between the highlight and the window, so
// it can be told apart from the hovered one (the highlight) next to it.
static uint32_t selected_color()
{
    uint32_t a = g_palette.highlight;
    uint32_t b = g_palette.panel_face;
    uint32_t mixed = 0xFF000000u;
    for (int shift = 0; shift <= 16; shift += 8)
    {
        uint32_t channel = (((a >> shift) & 0xFFu) + ((b >> shift) & 0xFFu)) / 2;
        mixed |= channel << shift;
    }
    return mixed;
}

// Window and widget colors, all from the palette (pushed in this order;
// THEME_COLOR_COUNT of them).
constexpr int THEME_COLOR_COUNT = 34;

static void push_theme_colors()
{
    ImGui::PushStyleColor(ImGuiCol_Text, to_imgui(g_palette.text));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, to_imgui(g_palette.text_disabled));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, to_imgui(g_palette.panel_face));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, to_imgui(g_palette.field_face));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, to_imgui(g_palette.panel_face));
    ImGui::PushStyleColor(ImGuiCol_Border, to_imgui(g_palette.panel_dark));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, to_imgui(g_palette.bar_face));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, to_imgui(g_palette.bar_face));
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, to_imgui(g_palette.bar_face));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, to_imgui(g_palette.bar_face));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, to_imgui(g_palette.field_face));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, to_imgui(g_palette.field_hover));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, to_imgui(g_palette.field_hover));
    ImGui::PushStyleColor(ImGuiCol_Button, to_imgui(g_palette.button_face));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, to_imgui(g_palette.highlight));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, to_imgui(g_palette.highlight));
    ImGui::PushStyleColor(ImGuiCol_Header, to_imgui(selected_color()));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, to_imgui(g_palette.highlight));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, to_imgui(g_palette.highlight));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, to_imgui(g_palette.text_highlight));
    // Scroll bars are drawn by retro_ui_scrollbars(); ImGui's are hidden.
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_NavCursor, to_imgui(g_palette.text_highlight));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, to_imgui(g_palette.bar_light));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, to_imgui(g_palette.highlight));
    ImGui::PushStyleColor(ImGuiCol_Tab, to_imgui(g_palette.bar_dark));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, to_imgui(g_palette.highlight));
    ImGui::PushStyleColor(ImGuiCol_TabSelected, to_imgui(g_palette.bar_face));
    ImGui::PushStyleColor(ImGuiCol_TabSelectedOverline, to_imgui(g_palette.text_highlight));
    ImGui::PushStyleColor(ImGuiCol_TabDimmed, to_imgui(g_palette.bar_dark));
    ImGui::PushStyleColor(ImGuiCol_TabDimmedSelected, to_imgui(g_palette.bar_face));
    ImGui::PushStyleColor(ImGuiCol_Separator, to_imgui(g_palette.panel_dark));
}

constexpr int THEME_VAR_COUNT = 12;   // the PushStyleVar calls in retro_ui_push_theme

void retro_ui_push_theme(ImFont* pixel_font, int32_t scale)
{
    float block = static_cast<float>(scale);
    ImGui::PushFont(pixel_font, PIXEL_FONT_IMGUI_UNIT * block);
    push_theme_colors();
    // Same metrics as the menu bar, in retro pixels.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f * block, 4.0f * block));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f * block, 2.0f * block));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f * block, 2.0f * block));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(3.0f * block, 2.0f * block));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 0.0f);
    // A 5-pixel scroll bar and a 4-pixel gap before it (the menu bar's
    // spacing), so it doesn't touch the buttons at the end of the rows.
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, (RETRO_UI_SCROLLBAR_WIDTH + RETRO_UI_SCROLLBAR_GAP) * block);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);   // bevels drawn instead
}

void retro_ui_pop_theme()
{
    ImGui::PopStyleVar(THEME_VAR_COUNT);
    ImGui::PopStyleColor(THEME_COLOR_COUNT);
    ImGui::PopFont();
}

// The title bar as a gradient (title_top -> title_bottom), with the title
// text drawn again on it (shadowed with the palette's shadows): ImGui's own
// title bar is one flat color.
static void draw_title_bar()
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->Flags & ImGuiWindowFlags_NoTitleBar)
    {
        return;
    }
    ImRect title = window->TitleBarRect();
    ImDrawList* draw_list = window->DrawList;
    draw_list->PushClipRect(title.Min, title.Max, false);
    ImU32 top = to_imgui_u32(g_palette.title_top);
    ImU32 bottom = to_imgui_u32(g_palette.title_bottom);
    draw_list->AddRectFilledMultiColor(title.Min, title.Max, top, top, bottom, bottom);
    const ImGuiStyle& style = ImGui::GetStyle();
    ImVec2 position(title.Min.x + style.FramePadding.x, title.Min.y + style.FramePadding.y);
    const char* end = ImGui::FindRenderedTextEnd(window->Name);
    draw_list->AddText(position, to_imgui_u32(g_palette.text), window->Name, end);
    draw_list->PopClipRect();
}

void retro_ui_begin_window(int32_t scale)
{
    use_nearest_sampler();
    draw_title_bar();
    draw_window_shadow(static_cast<float>(scale));
    draw_bevel(ImGui::GetWindowPos(), ImGui::GetWindowSize(), static_cast<float>(scale), g_palette.panel_light, g_palette.panel_light, true);
}

bool retro_ui_close_button()
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImRect title = window->TitleBarRect();
    float side = title.GetHeight();
    ImRect box(ImVec2(title.Max.x - side, title.Min.y), title.Max);
    ImGuiID id = window->GetID("#retro_close");

    // The title bar is outside the content's clip rect: open it up while we
    // add and draw the button there.
    ImGui::PushClipRect(title.Min, title.Max, false);
    bool pressed = false;
    if (ImGui::ItemAdd(box, id))
    {
        bool hovered = false;
        bool held = false;
        pressed = ImGui::ButtonBehavior(box, id, &hovered, &held);
        if (hovered || held)
        {
            window->DrawList->AddRectFilled(box.Min, box.Max, to_imgui_u32(g_palette.highlight));
        }
        ImVec2 text = ImGui::CalcTextSize("X");
        ImVec2 position(box.Min.x + static_cast<float>(static_cast<int32_t>((side - text.x) * 0.5f)),
                        box.Min.y + static_cast<float>(static_cast<int32_t>((side - text.y) * 0.5f)));
        window->DrawList->AddText(position, to_imgui_u32(hovered ? g_palette.text_highlight : g_palette.text), "X");
    }
    ImGui::PopClipRect();
    return pressed;
}

void retro_ui_end_window()
{
    use_linear_sampler();
}

// ---------------------------------------------------------------------------
// Combo with the font's arrow
// ---------------------------------------------------------------------------
// Checkbox with the font's check mark
// ---------------------------------------------------------------------------

// The raised edge of a button or check box: light top/left, dark bottom/right.
static void draw_raised(ImVec2 min, ImVec2 max, float block)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImU32 light = to_imgui_u32(g_palette.button_light);
    ImU32 dark = to_imgui_u32(g_palette.button_dark);
    draw_list->AddRectFilled(min, ImVec2(max.x, min.y + block), light);
    draw_list->AddRectFilled(min, ImVec2(min.x + block, max.y), light);
    draw_list->AddRectFilled(ImVec2(min.x, max.y - block), max, dark);
    draw_list->AddRectFilled(ImVec2(max.x - block, min.y), max, dark);
}

// A button's face: a vertical gradient (button_face -> button_bottom),
// upside down while pressed.
static void draw_button_face(ImVec2 min, ImVec2 max, bool pressed)
{
    ImU32 top = to_imgui_u32(pressed ? g_palette.button_bottom : g_palette.button_face);
    ImU32 bottom = to_imgui_u32(pressed ? g_palette.button_face : g_palette.button_bottom);
    ImGui::GetWindowDrawList()->AddRectFilledMultiColor(min, max, top, top, bottom, bottom);
}

// One retro pixel, from the font size (the pixel font is 8 units per line... of
// PIXEL_FONT_IMGUI_UNIT x scale): the theme's scale without passing it around.
static float retro_pixel()
{
    float pixel = ImGui::GetFontSize() / PIXEL_FONT_IMGUI_UNIT;
    return pixel >= 1.0f ? static_cast<float>(static_cast<int32_t>(pixel)) : 1.0f;
}

bool retro_ui_button_sized(const char* label, float width, float height)
{
    float block = retro_pixel();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    bool pressed = ImGui::Button(label, ImVec2(width, height));
    ImGui::PopStyleColor();
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    bool held = ImGui::IsItemActive() && ImGui::IsItemHovered();
    draw_button_face(min, max, held);
    if (!held)
    {
        draw_raised(min, max, block);   // pressed: flat
    }
    // The label, centered like ImGui's; down a pixel while pressed.
    const char* end = ImGui::FindRenderedTextEnd(label);
    ImVec2 text = ImGui::CalcTextSize(label, end);
    float x = static_cast<float>(static_cast<int32_t>(min.x + (max.x - min.x - text.x) * 0.5f));
    float y = static_cast<float>(static_cast<int32_t>(min.y + (max.y - min.y - text.y) * 0.5f)) + (held ? block : 0.0f);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    bool disabled = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0;
    draw_list->AddText(ImVec2(x, y), to_imgui_u32(disabled ? g_palette.text_disabled : g_palette.button_text), label, end);
    return pressed;
}

bool retro_ui_button(const char* label)
{
    return retro_ui_button_sized(label, 0.0f, 0.0f);
}

bool retro_ui_checkbox(const char* label, bool* value)
{
    // ImGui draws the box, label and highlight; its check mark is made
    // invisible and ours drawn in its place, on a raised box.
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, to_imgui(g_palette.button_face));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, to_imgui(g_palette.button_face));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, to_imgui(g_palette.button_face));
    bool changed = ImGui::Checkbox(label, value);
    ImGui::PopStyleColor(4);
    float side = ImGui::GetFrameHeight();
    ImVec2 box = ImGui::GetItemRectMin();
    draw_button_face(box, ImVec2(box.x + side, box.y + side), false);
    draw_raised(box, ImVec2(box.x + side, box.y + side), retro_pixel());
    if (*value)
    {
        ImVec2 mark = ImGui::CalcTextSize(PIXEL_FONT_UTF8_CHECK);
        // Whole pixels, so the mark stays sharp.
        ImVec2 position(box.x + static_cast<float>(static_cast<int32_t>((side - mark.x) * 0.5f)),
                        box.y + static_cast<float>(static_cast<int32_t>((side - mark.y) * 0.5f)));
        ImGui::GetWindowDrawList()->AddText(position, to_imgui_u32(g_palette.check), PIXEL_FONT_UTF8_CHECK);
    }
    return changed;
}

// The theme cursor: an arrow, 8 x 12 retro pixels, '#' filled.
static const char* const CURSOR_SHAPE[] = {
    "#.......",
    "##......",
    "###.....",
    "####....",
    "#####...",
    "######..",
    "#######.",
    "########",
    "####....",
    "##.##...",
    "#...##..",
    "....##..",
};
constexpr int CURSOR_ROWS = static_cast<int>(sizeof(CURSOR_SHAPE) / sizeof(CURSOR_SHAPE[0]));

void retro_ui_cursor(int32_t scale, bool visible)
{
    // The system's cursor is only hidden for ours (other themes keep it).
    // Through ImGui: its SDL backend sets the system cursor every frame, so
    // hiding it with SDL directly doesn't last.
    bool ours = g_palette.theme_cursor && visible;
    if (g_palette.theme_cursor)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    }
    ImGuiIO& io = ImGui::GetIO();
    if (!ours || !ImGui::IsMousePosValid(&io.MousePos))
    {
        return;
    }
    float block = static_cast<float>(scale);
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    ImVec2 tip(static_cast<float>(static_cast<int32_t>(io.MousePos.x)), static_cast<float>(static_cast<int32_t>(io.MousePos.y)));
    ImU32 shadow = (to_imgui_u32(g_palette.text_shadow) & ~IM_COL32_A_MASK) | (128u << IM_COL32_A_SHIFT);
    for (int pass = g_palette.shadows ? 0 : 1; pass < 2; pass++)
    {
        float offset = pass == 0 ? 2.0f * block : 0.0f;
        for (int row = 0; row < CURSOR_ROWS; row++)
        {
            // Top to bottom: the color to 60% of it (a gradient).
            float shade = 1.0f - 0.4f * static_cast<float>(row) / static_cast<float>(CURSOR_ROWS - 1);
            ImVec4 base = to_imgui(g_palette.cursor);
            ImU32 color = pass == 0 ? shadow
                                    : ImGui::ColorConvertFloat4ToU32(ImVec4(base.x * shade, base.y * shade, base.z * shade, 1.0f));
            for (int column = 0; CURSOR_SHAPE[row][column] != '\0'; column++)
            {
                if (CURSOR_SHAPE[row][column] == '#')
                {
                    ImVec2 min(tip.x + offset + static_cast<float>(column) * block, tip.y + offset + static_cast<float>(row) * block);
                    draw_list->AddRectFilled(min, ImVec2(min.x + block, min.y + block), color);
                }
            }
        }
    }
}

static void tooltip_text(const char* text)
{
    // The pixel font's letters all have the same advance: "M" stands for any.
    ImGui::PushTextWrapPos(ImGui::CalcTextSize("M").x * RETRO_UI_TOOLTIP_CHARACTERS);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
}

void retro_ui_item_tooltip(const char* text)
{
    if (ImGui::BeginItemTooltip())
    {
        tooltip_text(text);
        ImGui::EndTooltip();
    }
}

void retro_ui_tooltip(const char* text)
{
    if (ImGui::BeginTooltip())
    {
        tooltip_text(text);
        ImGui::EndTooltip();
    }
}

bool retro_ui_collapsing_header(const char* label, int flags)
{
    // ImGui draws the bar and the highlight; its arrow and label are made
    // invisible (both use the text color) and ours drawn in their place.
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    bool open = ImGui::CollapsingHeader(label, flags);
    ImGui::PopStyleColor();

    ImVec2 bar = ImGui::GetItemRectMin();
    const ImGuiStyle& style = ImGui::GetStyle();
    uint32_t color = to_imgui_u32(ImGui::IsItemHovered() ? g_palette.text_highlight : g_palette.text);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    // Whole pixels, so the glyphs stay sharp. ImGui puts the label after the
    // arrow's square (font size) and three frame paddings.
    float x = static_cast<float>(static_cast<int32_t>(bar.x + style.FramePadding.x));
    float y = static_cast<float>(static_cast<int32_t>(bar.y + style.FramePadding.y));
    draw_list->AddText(ImVec2(x, y), color, open ? PIXEL_FONT_UTF8_ARROW_DOWN : PIXEL_FONT_UTF8_ARROW_RIGHT);
    float text_x = static_cast<float>(static_cast<int32_t>(bar.x + ImGui::GetFontSize() + style.FramePadding.x * 3.0f));
    draw_list->AddText(ImVec2(text_x, y), color, label, ImGui::FindRenderedTextEnd(label));
    return open;
}

void retro_ui_push_field_text()
{
    ImGui::PushStyleColor(ImGuiCol_Text, to_imgui(g_palette.field_text));
}

void retro_ui_pop_field_text()
{
    ImGui::PopStyleColor();
}

void retro_ui_separator_text(const char* label)
{
    ImGui::PushStyleColor(ImGuiCol_Text, to_imgui(g_palette.section));
    ImGui::SeparatorText(label);
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------

bool retro_ui_begin_combo(const char* label, const char* preview)
{
    // Where the combo's frame goes, worked out before BeginCombo: when it
    // opens, the current window becomes the popup and the item rect is gone.
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 position = ImGui::GetCursorScreenPos();
    float width = ImGui::CalcItemWidth();
    float height = ImGui::GetFrameHeight();

    ImGui::PushStyleColor(ImGuiCol_Text, to_imgui(g_palette.field_text));   // the preview, in its field
    bool open = ImGui::BeginCombo(label, preview, ImGuiComboFlags_NoArrowButton);
    ImGui::PopStyleColor();

    // A square at the right end, button-colored, with the arrow glyph centered.
    ImVec2 square_min(position.x + width - height, position.y);
    ImVec2 square_max(position.x + width, position.y + height);
    draw_list->AddRectFilled(square_min, square_max, to_imgui_u32(open ? g_palette.highlight : g_palette.bar_face));
    ImVec2 arrow = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, PIXEL_FONT_UTF8_ARROW_DOWN);
    ImVec2 arrow_position(static_cast<float>(static_cast<int32_t>(square_min.x + (height - arrow.x) * 0.5f)),
                          static_cast<float>(static_cast<int32_t>(square_min.y + (height - arrow.y) * 0.5f)));
    draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), arrow_position, to_imgui_u32(g_palette.text),
                       PIXEL_FONT_UTF8_ARROW_DOWN);
    return open;
}

// ---------------------------------------------------------------------------
// Text shadows
// ---------------------------------------------------------------------------

// A copy of three vertices (one triangle) moved by `offset` and colored
// `shadow` (with the original's opacity), at the end of the vertex buffer;
// their indices relative to the command's vertex offset go to `indices`.
static void add_shadow_triangle(ImDrawList* list, const ImDrawCmd* command, const ImDrawIdx* triangle, float offset,
                                ImU32 shadow, ImVector<ImDrawIdx>* indices)
{
    for (int corner = 0; corner < 3; corner++)
    {
        ImDrawVert vertex = list->VtxBuffer[static_cast<int>(command->VtxOffset) + triangle[corner]];
        vertex.pos.x += offset;
        vertex.pos.y += offset;
        uint32_t alpha = (vertex.col >> IM_COL32_A_SHIFT) & 0xFFu;
        vertex.col = (shadow & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
        indices->push_back(static_cast<ImDrawIdx>(static_cast<unsigned int>(list->VtxBuffer.Size) - command->VtxOffset));
        list->VtxBuffer.push_back(vertex);
    }
}

// Is this triangle part of a letter (sampled from the font texture, not
// from its white pixel like every shape)?
static bool is_glyph(const ImDrawList* list, const ImDrawCmd* command, const ImDrawIdx* triangle, ImVec2 white)
{
    for (int corner = 0; corner < 3; corner++)
    {
        const ImDrawVert& vertex = list->VtxBuffer[static_cast<int>(command->VtxOffset) + triangle[corner]];
        if (vertex.uv.x != white.x || vertex.uv.y != white.y)
        {
            return true;
        }
    }
    return false;
}

// Can `count` more vertices be indexed from this command's vertex offset?
static bool shadow_fits(const ImDrawList* list, const ImDrawCmd* command, int count)
{
    if (sizeof(ImDrawIdx) >= 4)
    {
        return true;
    }
    return static_cast<unsigned int>(list->VtxBuffer.Size + count) - command->VtxOffset <= 65536u;
}

void retro_ui_text_shadows(ImDrawData* draw_data, int32_t scale)
{
    if (!g_palette.shadows || draw_data == nullptr)
    {
        return;
    }
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    ImVec2 white = atlas->TexUvWhitePixel;
    float offset = static_cast<float>(scale);
    ImU32 shadow = to_imgui_u32(g_palette.text_shadow);
    ImVector<ImDrawIdx> indices;
    for (int l = 0; l < draw_data->CmdLists.Size; l++)
    {
        ImDrawList* list = draw_data->CmdLists[l];
        indices.resize(0);
        indices.reserve(list->IdxBuffer.Size * 2);
        for (int c = 0; c < list->CmdBuffer.Size; c++)
        {
            ImDrawCmd* command = &list->CmdBuffer[c];
            unsigned int first = static_cast<unsigned int>(indices.Size);
            const ImDrawIdx* source = list->IdxBuffer.Data + command->IdxOffset;
            unsigned int count = command->ElemCount;
            bool font = command->UserCallback == nullptr && command->TexRef._TexData == atlas->TexData;
            // A run of letters (one piece of text) gets all its shadows first,
            // then the letters, so no shadow covers a letter of its own text.
            unsigned int run = 0;
            for (unsigned int i = 0; i <= count; i += 3)
            {
                bool glyph = font && i < count && is_glyph(list, command, source + i, white);
                if (glyph)
                {
                    continue;
                }
                if (i > run && shadow_fits(list, command, static_cast<int>(i - run)))
                {
                    for (unsigned int t = run; t < i; t += 3)
                    {
                        add_shadow_triangle(list, command, source + t, offset, shadow, &indices);
                    }
                }
                for (unsigned int t = run; t < i + 3 && t < count; t++)
                {
                    indices.push_back(source[t]);
                }
                run = i + 3;
            }
            command->IdxOffset = first;
            command->ElemCount = static_cast<unsigned int>(indices.Size) - first;
        }
        list->IdxBuffer.swap(indices);
    }
    int vertices = 0;
    int total_indices = 0;
    for (int l = 0; l < draw_data->CmdLists.Size; l++)
    {
        vertices += draw_data->CmdLists[l]->VtxBuffer.Size;
        total_indices += draw_data->CmdLists[l]->IdxBuffer.Size;
    }
    draw_data->TotalVtxCount = vertices;
    draw_data->TotalIdxCount = total_indices;
}

// ---------------------------------------------------------------------------
// Scroll bars
// ---------------------------------------------------------------------------

// One window's vertical scroll bar drawn over ImGui's: a sunken track in the
// field color and a raised grip like a button (pressed while dragged). The
// grip's size and place are worked out as ImGui does (ScrollbarEx).
static void draw_scrollbar(ImGuiWindow* window, float block)
{
    ImRect frame = ImGui::GetWindowScrollbarRect(window, ImGuiAxis_Y);
    // The gap on the left is left as the window's background.
    frame.Min.x = ImMin(frame.Min.x + RETRO_UI_SCROLLBAR_GAP * block, frame.Max.x);
    float width = frame.GetWidth();
    float height = frame.GetHeight();
    if (width <= 0.0f || height <= 0.0f || height < width)
    {
        return;
    }
    const ImGuiStyle& style = ImGui::GetStyle();
    ImRect track = frame;
    float padding = ImTrunc(ImMin(style.ScrollbarPadding, ImMin(width, height) * 0.5f));
    track.Expand(-padding);
    float size = track.GetHeight();
    if (size < 1.0f)
    {
        return;
    }
    float visible = window->InnerRect.Max.y - window->InnerRect.Min.y;
    float contents = window->ContentSize.y + window->WindowPadding.y * 2.0f;
    float whole = ImMax(ImMax(contents, visible), 1.0f);
    float grip = static_cast<float>(static_cast<int>(ImClamp(size * (visible / whole), ImMin(size, style.GrabMinSize), size)));
    float scroll_max = ImMax(1.0f, static_cast<float>(static_cast<int64_t>(contents) - static_cast<int64_t>(visible)));
    float ratio = ImSaturate(window->Scroll.y / scroll_max);
    float grip_y = static_cast<float>(static_cast<int>(track.Min.y + ratio * (size - grip)));
    bool held = ImGui::GetActiveID() == ImGui::GetWindowScrollbarID(window, ImGuiAxis_Y);

    ImDrawList* draw_list = window->DrawList;
    draw_list->PushClipRect(frame.Min, frame.Max, false);
    draw_list->PushTexture(ImGui::GetIO().Fonts->TexRef);
    // The track, sunken: dark top and left, light bottom and right.
    draw_list->AddRectFilled(frame.Min, frame.Max, to_imgui_u32(g_palette.field_face));
    ImU32 dark = to_imgui_u32(g_palette.panel_dark);
    ImU32 light = to_imgui_u32(g_palette.panel_light);
    draw_list->AddRectFilled(frame.Min, ImVec2(frame.Max.x, frame.Min.y + block), dark);
    draw_list->AddRectFilled(frame.Min, ImVec2(frame.Min.x + block, frame.Max.y), dark);
    draw_list->AddRectFilled(ImVec2(frame.Min.x, frame.Max.y - block), frame.Max, light);
    draw_list->AddRectFilled(ImVec2(frame.Max.x - block, frame.Min.y), frame.Max, light);
    // The grip.
    ImVec2 min(track.Min.x, grip_y);
    ImVec2 max(track.Max.x, grip_y + grip);
    ImU32 top = to_imgui_u32(held ? g_palette.button_bottom : g_palette.button_face);
    ImU32 bottom = to_imgui_u32(held ? g_palette.button_face : g_palette.button_bottom);
    draw_list->AddRectFilledMultiColor(min, max, top, top, bottom, bottom);
    if (!held)
    {
        ImU32 grip_light = to_imgui_u32(g_palette.button_light);
        ImU32 grip_dark = to_imgui_u32(g_palette.button_dark);
        draw_list->AddRectFilled(min, ImVec2(max.x, min.y + block), grip_light);
        draw_list->AddRectFilled(min, ImVec2(min.x + block, max.y), grip_light);
        draw_list->AddRectFilled(ImVec2(min.x, max.y - block), max, grip_dark);
        draw_list->AddRectFilled(ImVec2(max.x - block, min.y), max, grip_dark);
    }
    draw_list->PopTexture();
    draw_list->PopClipRect();
}

void retro_ui_scrollbars(int32_t scale)
{
    ImGuiContext& context = *GImGui;
    float block = static_cast<float>(scale);
    for (int i = 0; i < context.Windows.Size; i++)
    {
        ImGuiWindow* window = context.Windows[i];
        if (window->Active && !window->Hidden && !window->Collapsed && window->ScrollbarY)
        {
            draw_scrollbar(window, block);
        }
    }
}
