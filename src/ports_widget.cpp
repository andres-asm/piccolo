#include "ports_widget.h"

#include <cmath>
#include <cstdio>

#include "imgui.h"
#include "input_devices.h"
#include "piccolo_text.h"
#include "port_indicators.h"
#include "retro_ui.h"

// ---------------------------------------------------------------------------
// Layout, in retro pixels
// ---------------------------------------------------------------------------

// The tabs.
constexpr float TAB_WIDTH     = 96.0f;   // every tab, whatever it shows
constexpr float TAB_OVERLAP   = 24.0f;   // more than 4 tabs: each covers this much of the one before
constexpr float TABS_LEFT     = 4.0f;
constexpr float CARD_PADDING  = 5.0f;
constexpr float LINE          = 7.0f;    // a line of the pixel font (glyphs 1..5 of it)
constexpr float HEADER        = 9.0f;    // the first line: LEDs, P1, ping
constexpr float CHIP_HEIGHT   = 15.0f;   // a controller
constexpr float CHIP_LIP      = 2.0f;    // its bottom rows, darker (piccoletto's buttons)
constexpr float CHIP_PADDING  = 4.0f;
constexpr float GAP           = 3.0f;
constexpr float PEEK          = CARD_PADDING + HEADER;   // risen this far, only the first line shows
constexpr float STATUS_GAP    = 6.0f;    // between the tabs and the status line

// DEVICES.
constexpr float BOARD_MARGIN  = 8.0f;
constexpr float BOARD_HEADER  = 15.0f;
constexpr float BOARD_HINT    = 11.0f;
constexpr float BOARD_GAP     = 4.0f;
constexpr float COLUMN_MIN    = 100.0f;  // narrower than this, the columns scroll instead
constexpr float DONE_WIDTH    = 40.0f;
constexpr float ARROW_WIDTH   = 12.0f;

// Over the game: a light dim, and cards the game shows through (piccoletto's).
constexpr uint32_t DIM         = 0x50000000u;
constexpr uint32_t CARD_ALPHA  = 0xB0000000u;
constexpr uint32_t LIT_ALPHA   = 0xD8000000u;   // the tab of the port that changed

// Timing.
constexpr uint64_t POP_MS      = 3000;   // the tabs stay up this long after a change
constexpr uint64_t STAGGER_MS  = 35;     // each tab starts moving this much after the one before
constexpr float    EASE_RATE   = 14.0f;  // per second: how fast a tab closes in on where it's going

// The directions and BACK, as bits of PortsWidget::held.
constexpr uint8_t HELD_LEFT  = 1;
constexpr uint8_t HELD_RIGHT = 2;
constexpr uint8_t HELD_UP    = 4;
constexpr uint8_t HELD_DOWN  = 8;
constexpr uint8_t HELD_BACK  = 16;

// ---------------------------------------------------------------------------
// Who is where
// ---------------------------------------------------------------------------

// A controller the widget shows: its device connected, not a mouse, not ignored.
static const InputDevice* shown_device(const InputSystem* input, uint32_t controller)
{
    if (controller >= input->controllers.size())
    {
        return nullptr;
    }
    const InputDevice* device = input_devices_find_const(&input->devices, input->controllers[controller].device_id);
    if (device == nullptr || !device->connected || device->kind == InputDeviceKind::mouse || input_devices_ignored(device))
    {
        return nullptr;
    }
    return device;
}

static uint64_t mix(uint64_t summary, uint64_t value)
{
    return (summary ^ value) * 1099511628211ull;
}

static uint64_t port_summary(const InputSystem* input, uint32_t port)
{
    uint64_t summary = 1469598103934665603ull;
    const std::vector<uint32_t>* members = &input->ports[port];
    for (size_t m = 0; m < members->size(); m++)
    {
        const InputDevice* device = shown_device(input, (*members)[m]);
        if (device != nullptr)
        {
            summary = mix(mix(summary, (*members)[m]), device->id);
        }
    }
    return summary;
}

static uint64_t out_summary(const InputSystem* input)
{
    uint64_t summary = 1469598103934665603ull;
    for (size_t c = 0; c < input->controllers.size(); c++)
    {
        uint32_t controller = static_cast<uint32_t>(c);
        const InputDevice* device = shown_device(input, controller);
        if (device != nullptr && input_controller_port(input, controller) == INPUT_NO_PORT)
        {
            summary = mix(mix(summary, controller), device->id);
        }
    }
    return summary;
}

// A remote pad: whose (the peer), or false.
static bool remote_peer(const InputNet* net, uint32_t device_id, uint32_t* peer)
{
    if (net == nullptr)
    {
        return false;
    }
    for (uint32_t owner = 0; owner < NET_MAX_PEERS; owner++)
    {
        for (uint32_t pad = 0; pad < NET_INPUT_MAX_PADS; pad++)
        {
            if (net->pads[owner][pad].active && net->pads[owner][pad].device == device_id)
            {
                *peer = owner;
                return true;
            }
        }
    }
    return false;
}

static void add_device(std::vector<PortsWidgetDevice>* devices, const std::vector<PortsWidgetDevice>* before,
                       const InputSystem* input, const InputNet* net, uint32_t controller, const InputDevice* device)
{
    PortsWidgetDevice entry;
    entry.controller = controller;
    entry.label = input_controller_label(input, controller);
    entry.remote = remote_peer(net, device->id, &entry.peer);
    // The last ping it had (the frontend refreshes it every so often).
    for (size_t i = 0; i < before->size(); i++)
    {
        if ((*before)[i].controller == controller && !(*before)[i].gone)
        {
            entry.ping_known = (*before)[i].ping_known;
            entry.ping_ms = (*before)[i].ping_ms;
        }
    }
    devices->push_back(entry);
}

static bool has_controller(const std::vector<PortsWidgetDevice>* devices, uint32_t controller, bool gone)
{
    for (size_t i = 0; i < devices->size(); i++)
    {
        if ((*devices)[i].controller == controller && (*devices)[i].gone == gone)
        {
            return true;
        }
    }
    return false;
}

// A port's list again (`port` INPUT_NO_PORT: OUT). Controllers that went
// away stay listed (greyed) until the tabs go down, unless they're back.
static void rebuild(PortsWidgetColumn* column, const InputSystem* input, const InputNet* net, int32_t port)
{
    std::vector<PortsWidgetDevice> before;
    before.swap(column->devices);
    if (port >= 0)
    {
        const std::vector<uint32_t>* members = &input->ports[port];
        for (size_t m = 0; m < members->size(); m++)
        {
            const InputDevice* device = shown_device(input, (*members)[m]);
            if (device != nullptr)
            {
                add_device(&column->devices, &before, input, net, (*members)[m], device);
            }
        }
        column->summary = port_summary(input, static_cast<uint32_t>(port));
    }
    else
    {
        for (size_t c = 0; c < input->controllers.size(); c++)
        {
            uint32_t controller = static_cast<uint32_t>(c);
            const InputDevice* device = shown_device(input, controller);
            if (device != nullptr && input_controller_port(input, controller) == INPUT_NO_PORT)
            {
                add_device(&column->devices, &before, input, net, controller, device);
            }
        }
        column->summary = out_summary(input);
    }
    for (size_t i = 0; i < before.size(); i++)
    {
        if (before[i].gone && !has_controller(&column->devices, before[i].controller, false))
        {
            column->devices.push_back(before[i]);
        }
    }
}

static void drop_gone(PortsWidgetColumn* column)
{
    for (size_t i = column->devices.size(); i > 0; i--)
    {
        if (column->devices[i - 1].gone)
        {
            column->devices.erase(column->devices.begin() + static_cast<std::ptrdiff_t>(i - 1));
        }
    }
}

static bool has_live(const PortsWidgetColumn* column)
{
    for (size_t i = 0; i < column->devices.size(); i++)
    {
        if (!column->devices[i].gone)
        {
            return true;
        }
    }
    return false;
}

static void forget(PortsWidget* widget)
{
    widget->port_count = 0;
    widget->known = false;
    widget->open = false;
    widget->popped_ms = 0;
    for (uint32_t p = 0; p < INPUT_PORT_COUNT; p++)
    {
        widget->ports[p].devices.clear();
        widget->changed[p] = false;
        widget->height[p] = 0.0f;
        widget->target[p] = 0.0f;
    }
    widget->out.devices.clear();
}

void ports_widget_update(PortsWidget* widget, const InputSystem* input, const InputNet* net, const InputFrame* frame,
                         uint32_t port_count, uint64_t now_ms)
{
    if (port_count == 0)
    {
        if (widget->known || widget->open)
        {
            forget(widget);
        }
        return;
    }
    if (port_count > INPUT_PORT_COUNT)
    {
        port_count = INPUT_PORT_COUNT;
    }
    // A new game, or another number of ports: taken as they are, no tabs.
    if (!widget->known || port_count != widget->port_count)
    {
        widget->port_count = port_count;
        widget->known = true;
        for (uint32_t p = 0; p < INPUT_PORT_COUNT; p++)
        {
            widget->ports[p].devices.clear();
            rebuild(&widget->ports[p], input, net, static_cast<int32_t>(p));
        }
        widget->out.devices.clear();
        rebuild(&widget->out, input, net, INPUT_NO_PORT);
        return;
    }
    bool changed_now[INPUT_PORT_COUNT] = {};
    bool any = false;
    for (uint32_t p = 0; p < INPUT_PORT_COUNT; p++)
    {
        if (port_summary(input, p) != widget->ports[p].summary)
        {
            rebuild(&widget->ports[p], input, net, static_cast<int32_t>(p));
            changed_now[p] = p < port_count;   // (ports not shown change unseen)
            any = any || changed_now[p];
        }
    }
    if (out_summary(input) != widget->out.summary)
    {
        rebuild(&widget->out, input, net, INPUT_NO_PORT);
    }
    // A controller whose device went away: greyed on the port it left.
    if (frame->left_port >= 0 && frame->left_port < static_cast<int32_t>(port_count) && frame->left_controller >= 0 &&
        !widget->open)
    {
        uint32_t controller = static_cast<uint32_t>(frame->left_controller);
        PortsWidgetColumn* column = &widget->ports[frame->left_port];
        if (!has_controller(&column->devices, controller, true) && controller < input->controllers.size())
        {
            PortsWidgetDevice gone;
            gone.controller = controller;
            gone.label = input_controller_label(input, controller);
            gone.gone = true;
            column->devices.push_back(gone);
        }
        changed_now[frame->left_port] = true;
        any = true;
    }
    if (any && !widget->open)
    {
        if (widget->popped_ms == 0)
        {
            for (uint32_t p = 0; p < INPUT_PORT_COUNT; p++)
            {
                widget->changed[p] = false;
            }
        }
        for (uint32_t p = 0; p < INPUT_PORT_COUNT; p++)
        {
            widget->changed[p] = widget->changed[p] || changed_now[p];
        }
        widget->popped_ms = now_ms;   // (another change keeps them up longer)
    }
    if (widget->popped_ms != 0 && now_ms - widget->popped_ms >= POP_MS)
    {
        widget->popped_ms = 0;   // going down; the greyed ones go once they're down (draw)
    }
}

bool ports_widget_tabs_visible(const PortsWidget* widget)
{
    for (uint32_t p = 0; p < widget->port_count; p++)
    {
        if (widget->height[p] > 0.0f || widget->target[p] > 0.0f)
        {
            return true;
        }
    }
    return false;
}

static void set_ping_in(PortsWidgetColumn* column, uint32_t peer, bool known, uint32_t ping_ms)
{
    for (size_t i = 0; i < column->devices.size(); i++)
    {
        PortsWidgetDevice* device = &column->devices[i];
        if (device->remote && device->peer == peer)
        {
            device->ping_known = known;
            device->ping_ms = ping_ms;
        }
    }
}

void ports_widget_set_ping(PortsWidget* widget, uint32_t peer, bool known, uint32_t ping_ms)
{
    for (uint32_t p = 0; p < INPUT_PORT_COUNT; p++)
    {
        set_ping_in(&widget->ports[p], peer, known, ping_ms);
    }
    set_ping_in(&widget->out, peer, known, ping_ms);
}

// ---------------------------------------------------------------------------
// Drawing pieces (window pixels; `block` = one retro pixel)
// ---------------------------------------------------------------------------

static uint32_t with_alpha(uint32_t argb, uint32_t alpha)
{
    return (argb & 0x00FFFFFFu) | alpha;
}

// A button's lip: its face, darker.
static uint32_t darker(uint32_t argb)
{
    uint32_t out = argb & 0xFF000000u;
    for (int shift = 0; shift <= 16; shift += 8)
    {
        uint32_t channel = ((argb >> shift) & 0xFFu) * 7u / 10u;
        out |= channel << shift;
    }
    return out;
}

// One LED per port, the port's own lit when someone is on it. Returns the width.
static float draw_leds(float x, float y, float block, uint32_t count, uint32_t port, bool lit)
{
    const RetroUiPalette* palette = retro_ui_palette();
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t color = lit && i == port ? PORT_LED_ON : palette->panel_dark;
        retro_ui_layer_rect(x + static_cast<float>(i) * (PORT_LED_SIZE + PORT_LED_GAP) * block, y + block,
                            PORT_LED_SIZE * block, PORT_LED_SIZE * block, color);
    }
    return count == 0 ? 0.0f : (static_cast<float>(count) * (PORT_LED_SIZE + PORT_LED_GAP) - PORT_LED_GAP) * block;
}

static float ping_bars_width(float block)
{
    return (static_cast<float>(PING_BARS) * PING_BAR_WIDTH + static_cast<float>(PING_BARS - 1) * PING_BAR_GAP) * block;
}

// Three rising bars, bottoms level with the letters', ending at `right`.
static void draw_ping_bars(float right, float y, float block, bool known, uint32_t ping_ms)
{
    const RetroUiPalette* palette = retro_ui_palette();
    uint32_t lit = known ? ping_bars_lit(ping_ms) : 0;
    uint32_t color = known ? ping_color(ping_ms) : palette->panel_dark;
    float x = right - ping_bars_width(block);
    float bottom = y + 6.0f * block;
    for (uint32_t i = 0; i < PING_BARS; i++)
    {
        float height = ping_bar_height(i) * block;
        retro_ui_layer_rect(x + static_cast<float>(i) * (PING_BAR_WIDTH + PING_BAR_GAP) * block, bottom - height,
                            PING_BAR_WIDTH * block, height, i < lit ? color : palette->panel_dark);
    }
}

// The bars and "12 MS" ending at `right`: the first remote pad of the list's.
static void draw_ping(const std::vector<PortsWidgetDevice>* devices, const Localization* strings, float right, float y,
                      float block)
{
    for (size_t i = 0; i < devices->size(); i++)
    {
        const PortsWidgetDevice* device = &(*devices)[i];
        if (!device->remote || device->gone)
        {
            continue;
        }
        float x = right;
        if (device->ping_known)
        {
            char text[24];
            std::snprintf(text, sizeof(text), piccolo_text(strings, Str::ports_ping), static_cast<unsigned>(device->ping_ms));
            x -= retro_ui_layer_text_width(text);
            retro_ui_layer_text(x, y, text, ping_color(device->ping_ms));
            x -= 3.0f * block;
        }
        draw_ping_bars(x, y, block, device->ping_known, device->ping_ms);
        return;
    }
}

// A controller as a flat button with a lip (the picked one in the highlight).
static void draw_chip(const PortsWidgetDevice* device, const Localization* strings, float x, float y, float width,
                      float block, bool picked)
{
    const RetroUiPalette* palette = retro_ui_palette();
    uint32_t face = device->gone ? palette->field_face : (picked ? palette->highlight : palette->button_face);
    uint32_t text = device->gone ? palette->text_disabled : (picked ? palette->text_highlight : palette->button_text);
    float height = CHIP_HEIGHT * block;
    float lip = CHIP_LIP * block;
    retro_ui_layer_rect(x, y, width, height - lip, face);
    retro_ui_layer_rect(x, y + height - lip, width, lip, darker(face));
    float text_y = y + std::floor((CHIP_HEIGHT - CHIP_LIP - LINE) * 0.5f) * block;
    float right = x + width - CHIP_PADDING * block;
    if (device->gone)
    {
        const char* off = piccolo_text(strings, Str::ports_off);
        right -= retro_ui_layer_text_width(off);
        retro_ui_layer_text(right, text_y, off, palette->text_disabled);
        right -= 3.0f * block;
    }
    else if (device->remote)
    {
        draw_ping_bars(right, text_y, block, device->ping_known, device->ping_ms);
        right -= ping_bars_width(block) + 3.0f * block;
    }
    float left = x + CHIP_PADDING * block;
    retro_ui_layer_clip(left, y, right - left, height);
    retro_ui_layer_text(left, text_y, device->label.c_str(), text);
    retro_ui_layer_unclip();
}

// How tall a column's card is: padding, its first line, its controllers
// (or NOBODY), padding.
static float card_height(const PortsWidgetColumn* column)
{
    size_t rows = column->devices.size();
    float content = rows == 0 ? LINE : static_cast<float>(rows) * (CHIP_HEIGHT + GAP) - GAP;
    return CARD_PADDING + HEADER + GAP + content + CARD_PADDING;
}

// ---------------------------------------------------------------------------
// Tabs
// ---------------------------------------------------------------------------

static float tab_step(uint32_t count)
{
    return count > 4 ? TAB_WIDTH - TAB_OVERLAP : TAB_WIDTH;
}

float ports_widget_tabs_width(const PortsWidget* widget)
{
    if (widget->port_count == 0 || widget->open)
    {
        return 0.0f;
    }
    return TABS_LEFT + static_cast<float>(widget->port_count - 1) * tab_step(widget->port_count) + TAB_WIDTH + STATUS_GAP;
}

static void draw_tab(const PortsWidget* widget, uint32_t port, const Localization* strings, float block, float screen_height)
{
    const RetroUiPalette* palette = retro_ui_palette();
    const PortsWidgetColumn* column = &widget->ports[port];
    bool lit = widget->changed[port] && widget->popped_ms != 0;
    float x = (TABS_LEFT + static_cast<float>(port) * tab_step(widget->port_count)) * block;
    float y = screen_height - widget->height[port] * block;
    float width = TAB_WIDTH * block;
    // The whole card; what hasn't risen yet is below the window's edge.
    retro_ui_layer_rect(x, y, width, card_height(column) * block, with_alpha(palette->panel_face, lit ? LIT_ALPHA : CARD_ALPHA));

    // First line: the LEDs, P1, a remote pad's ping.
    float line_x = x + CARD_PADDING * block;
    float line_y = y + CARD_PADDING * block;
    float leds = draw_leds(line_x, line_y, block, widget->port_count, port, has_live(column));
    char name[16];
    std::snprintf(name, sizeof(name), piccolo_text(strings, Str::ports_tab), static_cast<unsigned>(port + 1));
    retro_ui_layer_text(line_x + leds + 4.0f * block, line_y, name, lit ? palette->text_highlight : palette->text_disabled);
    draw_ping(&column->devices, strings, x + width - CARD_PADDING * block, line_y, block);

    // Who is on it.
    float chip_y = line_y + (HEADER + GAP) * block;
    float chip_width = width - 2.0f * CARD_PADDING * block;
    if (column->devices.empty())
    {
        retro_ui_layer_text(line_x, chip_y, piccolo_text(strings, Str::ports_nobody), palette->text_disabled);
    }
    for (size_t i = 0; i < column->devices.size(); i++)
    {
        draw_chip(&column->devices[i], strings, line_x, chip_y, chip_width, block, false);
        chip_y += (CHIP_HEIGHT + GAP) * block;
    }
}

void ports_widget_draw_tabs(PortsWidget* widget, ImFont* pixel_font, int32_t scale, const Localization* strings,
                            bool menu_shown, uint64_t now_ms)
{
    uint32_t count = widget->port_count;
    float dt = widget->last_draw_ms == 0 || now_ms < widget->last_draw_ms
                   ? 0.0f
                   : static_cast<float>(now_ms - widget->last_draw_ms) / 1000.0f;
    dt = dt > 0.1f ? 0.1f : dt;   // (a stall doesn't jump them)
    widget->last_draw_ms = now_ms;
    // Where each tab goes: hidden, its first line, or whole.
    bool any_up = false;
    for (uint32_t p = 0; p < count; p++)
    {
        float target = 0.0f;
        if (!widget->open && widget->popped_ms != 0)
        {
            target = widget->changed[p] ? card_height(&widget->ports[p]) : PEEK;
        }
        else if (!widget->open && menu_shown)
        {
            target = PEEK;
        }
        if (target != widget->target[p])
        {
            widget->target[p] = target;
            widget->target_ms[p] = now_ms + static_cast<uint64_t>(p) * STAGGER_MS;
        }
        if (now_ms >= widget->target_ms[p])
        {
            float distance = widget->target[p] - widget->height[p];
            widget->height[p] += distance * (1.0f - std::exp(-EASE_RATE * dt));
            if (std::fabs(widget->target[p] - widget->height[p]) < 0.25f)
            {
                widget->height[p] = widget->target[p];
            }
        }
        any_up = any_up || widget->height[p] > 0.0f;
    }
    // Down to their first line (or gone): the greyed controllers go now.
    bool settled = widget->popped_ms == 0;
    for (uint32_t p = 0; p < count && settled; p++)
    {
        settled = widget->height[p] <= PEEK;
    }
    if (settled)
    {
        for (uint32_t p = 0; p < INPUT_PORT_COUNT; p++)
        {
            drop_gone(&widget->ports[p]);
        }
    }
    if (!any_up)
    {
        return;
    }
    float block = static_cast<float>(scale);
    float screen_height = ImGui::GetIO().DisplaySize.y;
    retro_ui_begin_layer("##ports_tabs", pixel_font, scale, false);
    // The ones that changed last, so they're on top where tabs overlap.
    for (uint32_t pass = 0; pass < 2; pass++)
    {
        for (uint32_t p = 0; p < count; p++)
        {
            bool lit = widget->changed[p] && widget->popped_ms != 0;
            if ((pass == 1) == lit && widget->height[p] > 0.0f)
            {
                draw_tab(widget, p, strings, block, screen_height);
            }
        }
    }
    retro_ui_end_layer();
}

// ---------------------------------------------------------------------------
// DEVICES
// ---------------------------------------------------------------------------

static uint8_t held_bits(const InputSystem* input, uint32_t controller)
{
    uint32_t bits = 0;
    bits |= input_controller_action_held(input, controller, InputAction::ui_left) ? HELD_LEFT : 0u;
    bits |= input_controller_action_held(input, controller, InputAction::ui_right) ? HELD_RIGHT : 0u;
    bits |= input_controller_action_held(input, controller, InputAction::ui_up) ? HELD_UP : 0u;
    bits |= input_controller_action_held(input, controller, InputAction::ui_down) ? HELD_DOWN : 0u;
    bits |= input_controller_action_held(input, controller, InputAction::ui_back) ? HELD_BACK : 0u;
    return static_cast<uint8_t>(bits);
}

// The columns in the board's order: OUT, then the ports.
static PortsWidgetColumn* board_column(PortsWidget* widget, uint32_t index)
{
    return index == 0 ? &widget->out : &widget->ports[index - 1];
}

static bool find_picked(PortsWidget* widget, uint32_t* column_index, size_t* row)
{
    for (uint32_t c = 0; c <= widget->port_count; c++)
    {
        PortsWidgetColumn* column = board_column(widget, c);
        for (size_t i = 0; i < column->devices.size(); i++)
        {
            if (column->devices[i].controller == widget->picked)
            {
                *column_index = c;
                *row = i;
                return true;
            }
        }
    }
    return false;
}

// UP / DOWN: the next controller in the board's order (wrapping around).
static void pick_next(PortsWidget* widget, int32_t direction)
{
    std::vector<uint32_t> order;   // (a few entries, only on a press)
    for (uint32_t c = 0; c <= widget->port_count; c++)
    {
        PortsWidgetColumn* column = board_column(widget, c);
        for (size_t i = 0; i < column->devices.size(); i++)
        {
            order.push_back(column->devices[i].controller);
        }
    }
    if (order.empty())
    {
        return;
    }
    int32_t at = -1;
    for (size_t i = 0; i < order.size(); i++)
    {
        at = order[i] == widget->picked ? static_cast<int32_t>(i) : at;
    }
    int32_t size = static_cast<int32_t>(order.size());
    int32_t next = at < 0 ? 0 : ((at + direction) % size + size) % size;
    widget->picked = order[static_cast<size_t>(next)];
}

// A controller one port to the left (OUT before port 1) or right, on that
// port only.
static void move_to(InputSystem* input, uint32_t controller, int32_t to)
{
    for (uint32_t p = 0; p < INPUT_PORT_COUNT; p++)
    {
        input_port_remove(input, p, controller);
    }
    if (to >= 0)
    {
        input_port_add(input, static_cast<uint32_t>(to), controller);
    }
}

static void move_by(InputSystem* input, uint32_t controller, int32_t direction, uint32_t port_count)
{
    int32_t from = input_controller_port(input, controller);   // INPUT_NO_PORT (-1) = OUT
    int32_t to = from + direction;
    to = to < INPUT_NO_PORT ? INPUT_NO_PORT : to;
    to = to >= static_cast<int32_t>(port_count) ? static_cast<int32_t>(port_count) - 1 : to;
    if (to != from)
    {
        move_to(input, controller, to);
    }
}

void ports_widget_open(PortsWidget* widget, const InputSystem* input)
{
    widget->open = true;
    widget->popped_ms = 0;
    for (uint32_t p = 0; p < INPUT_PORT_COUNT; p++)
    {
        drop_gone(&widget->ports[p]);
    }
    // What's held now (the hotkey that opened it) does nothing until let go.
    widget->held.assign(input->controllers.size(), 0);
    for (size_t c = 0; c < input->controllers.size(); c++)
    {
        widget->held[c] = held_bits(input, static_cast<uint32_t>(c));
    }
    // Port 1's first gamepad picked (else whoever is first there).
    widget->picked = UINT32_MAX;
    const std::vector<PortsWidgetDevice>* first = &widget->ports[0].devices;
    for (size_t i = 0; i < first->size() && widget->picked == UINT32_MAX; i++)
    {
        const InputDevice* device = shown_device(input, (*first)[i].controller);
        if (device != nullptr && device->kind == InputDeviceKind::gamepad)
        {
            widget->picked = (*first)[i].controller;
        }
    }
    if (widget->picked == UINT32_MAX && !first->empty())
    {
        widget->picked = (*first)[0].controller;
    }
    widget->scroll = -1.0f;   // placed at once on the first draw
}

void ports_widget_close(PortsWidget* widget)
{
    widget->open = false;
}

void ports_widget_board_update(PortsWidget* widget, InputSystem* input)
{
    if (!widget->open)
    {
        return;
    }
    size_t count = input->controllers.size();
    if (widget->held.size() != count)
    {
        // Controllers came or went: the new ones start as held (only when the count changes).
        size_t before = widget->held.size();
        widget->held.resize(count, 0);
        for (size_t c = before; c < count; c++)
        {
            widget->held[c] = held_bits(input, static_cast<uint32_t>(c));
        }
    }
    uint32_t column_index = 0;
    size_t row = 0;
    if (!find_picked(widget, &column_index, &row))
    {
        widget->picked = UINT32_MAX;
    }
    for (size_t c = 0; c < count && widget->open; c++)
    {
        uint32_t controller = static_cast<uint32_t>(c);
        uint8_t bits = held_bits(input, controller);
        uint8_t pressed = static_cast<uint8_t>(bits & ~widget->held[c]);
        widget->held[c] = bits;
        const InputDevice* device = shown_device(input, controller);
        // A remote pad moves from its own device's DEVICES (never the interface here).
        if (pressed == 0 || device == nullptr || device->game_only)
        {
            continue;
        }
        int32_t direction = (pressed & HELD_LEFT) != 0 ? -1 : ((pressed & HELD_RIGHT) != 0 ? 1 : 0);
        if (input_port_has(input, 0, controller))
        {
            // Port 1: picks anyone, moves the pick, closes.
            if ((pressed & HELD_BACK) != 0)
            {
                ports_widget_close(widget);
                break;
            }
            if ((pressed & HELD_UP) != 0)
            {
                pick_next(widget, -1);
            }
            if ((pressed & HELD_DOWN) != 0)
            {
                pick_next(widget, 1);
            }
            if (direction != 0)
            {
                if (widget->picked == UINT32_MAX)
                {
                    widget->picked = controller;
                }
                move_by(input, widget->picked, direction, widget->port_count);
            }
        }
        else if (direction != 0)
        {
            move_by(input, controller, direction, widget->port_count);
        }
    }
}

static bool clicked_in(float x, float y, float width, float height)
{
    const ImGuiIO& io = ImGui::GetIO();
    return io.MouseClicked[0] && io.MousePos.x >= x && io.MousePos.x < x + width && io.MousePos.y >= y &&
           io.MousePos.y < y + height;
}

void ports_widget_draw_board(PortsWidget* widget, InputSystem* input, ImFont* pixel_font, int32_t scale,
                             const Localization* strings)
{
    if (!widget->open || widget->port_count == 0)
    {
        return;
    }
    const RetroUiPalette* palette = retro_ui_palette();
    const ImGuiIO& io = ImGui::GetIO();
    float block = static_cast<float>(scale);
    float screen_width = io.DisplaySize.x;
    float screen_height = io.DisplaySize.y;
    retro_ui_begin_layer("##ports_board", pixel_font, scale, true);
    retro_ui_layer_rect(0.0f, 0.0f, screen_width, screen_height, DIM);

    // The title bar with DONE, and the hint under it (piccoletto's).
    float margin = BOARD_MARGIN * block;
    float gap = BOARD_GAP * block;
    float inner_width = screen_width - 2.0f * margin;
    float header_y = margin;
    float header_height = BOARD_HEADER * block;
    float done_width = DONE_WIDTH * block;
    float text_offset = std::floor((BOARD_HEADER - LINE) * 0.5f) * block;
    retro_ui_layer_rect(margin, header_y, inner_width - done_width - gap, header_height, with_alpha(palette->panel_dark, CARD_ALPHA));
    retro_ui_layer_text(margin + 6.0f * block, header_y + text_offset, piccolo_text(strings, Str::ports_title),
                        palette->text_highlight);
    float done_x = margin + inner_width - done_width;
    retro_ui_layer_rect(done_x, header_y, done_width, header_height - CHIP_LIP * block, palette->highlight);
    retro_ui_layer_rect(done_x, header_y + header_height - CHIP_LIP * block, done_width, CHIP_LIP * block, darker(palette->highlight));
    const char* done = piccolo_text(strings, Str::ports_done);
    retro_ui_layer_text(done_x + std::floor((done_width - retro_ui_layer_text_width(done)) * 0.5f), header_y + text_offset - block,
                        done, palette->text_highlight);
    bool close = clicked_in(done_x, header_y, done_width, header_height);
    float hint_y = header_y + header_height + gap;
    float hint_height = BOARD_HINT * block;
    retro_ui_layer_rect(margin, hint_y, inner_width, hint_height, with_alpha(palette->panel_dark, CARD_ALPHA));
    retro_ui_layer_text(margin + 6.0f * block, hint_y + std::floor((BOARD_HINT - LINE) * 0.5f) * block,
                        piccolo_text(strings, Str::ports_help), palette->text_disabled);

    // The columns: OUT and the ports, side by side; scrolled when they don't fit.
    float content_y = hint_y + hint_height + gap;
    float content_height = screen_height - content_y - margin;
    uint32_t columns = widget->port_count + 1;
    float column_width = (inner_width - static_cast<float>(columns - 1) * gap) / static_cast<float>(columns);
    float minimum = COLUMN_MIN * block;
    column_width = column_width < minimum ? minimum : std::floor(column_width);
    float total = static_cast<float>(columns) * column_width + static_cast<float>(columns - 1) * gap;
    float max_scroll = total > inner_width ? total - inner_width : 0.0f;
    uint32_t picked_column = 0;
    size_t picked_row = 0;
    bool has_pick = find_picked(widget, &picked_column, &picked_row);
    if (has_pick)
    {
        // The picked controller's column in the middle (as far as it goes).
        float center = static_cast<float>(picked_column) * (column_width + gap) + column_width * 0.5f;
        widget->scroll_target = center - inner_width * 0.5f;
    }
    widget->scroll_target = widget->scroll_target < 0.0f ? 0.0f : (widget->scroll_target > max_scroll ? max_scroll : widget->scroll_target);
    if (widget->scroll < 0.0f)
    {
        widget->scroll = widget->scroll_target;
    }
    widget->scroll += (widget->scroll_target - widget->scroll) * 0.25f;
    if (std::fabs(widget->scroll_target - widget->scroll) < 0.5f)
    {
        widget->scroll = widget->scroll_target;
    }
    // Cards as tall as the tallest, so they line up.
    float tallest = 0.0f;
    for (uint32_t c = 0; c < columns; c++)
    {
        float height = card_height(board_column(widget, c));
        tallest = height > tallest ? height : tallest;
    }
    float card = tallest * block;
    card = card > content_height ? content_height : card;

    int32_t move_column = -1;   // a click on a column with a controller picked
    retro_ui_layer_clip(margin, content_y, inner_width, content_height);
    for (uint32_t c = 0; c < columns; c++)
    {
        const PortsWidgetColumn* column = board_column(widget, c);
        float x = margin + static_cast<float>(c) * (column_width + gap) - std::floor(widget->scroll);
        if (x + column_width < margin || x > margin + inner_width)
        {
            continue;
        }
        retro_ui_layer_rect(x, content_y, column_width, card, with_alpha(palette->panel_face, CARD_ALPHA));
        float line_x = x + CARD_PADDING * block;
        float line_y = content_y + CARD_PADDING * block;
        char title[32];
        if (c == 0)
        {
            std::snprintf(title, sizeof(title), "%s", piccolo_text(strings, Str::ports_out));
            retro_ui_layer_text(line_x, line_y, title, palette->text_disabled);
        }
        else
        {
            float leds = draw_leds(line_x, line_y, block, widget->port_count, c - 1, has_live(column));
            std::snprintf(title, sizeof(title), "%s %u", piccolo_text(strings, Str::input_port), static_cast<unsigned>(c));
            retro_ui_layer_text(line_x + leds + 4.0f * block, line_y, title, palette->text_disabled);
            draw_ping(&column->devices, strings, x + column_width - CARD_PADDING * block, line_y, block);
        }
        float chip_y = line_y + (HEADER + GAP) * block;
        float chip_width = column_width - 2.0f * CARD_PADDING * block;
        bool chip_clicked = false;
        if (column->devices.empty())
        {
            retro_ui_layer_text(line_x, chip_y, piccolo_text(strings, Str::ports_nobody), palette->text_disabled);
        }
        for (size_t i = 0; i < column->devices.size(); i++)
        {
            const PortsWidgetDevice* device = &column->devices[i];
            draw_chip(device, strings, line_x, chip_y, chip_width, block, device->controller == widget->picked);
            if (clicked_in(line_x, chip_y, chip_width, CHIP_HEIGHT * block))
            {
                widget->picked = device->controller;   // a click picks it
                chip_clicked = true;
            }
            chip_y += (CHIP_HEIGHT + GAP) * block;
        }
        if (!chip_clicked && has_pick && clicked_in(x, content_y, column_width, content_height))
        {
            move_column = static_cast<int32_t>(c);
        }
    }
    retro_ui_layer_unclip();

    // Arrows where there are more columns (the pixel font's triangles,
    // PIXEL_FONT_ARROW_LEFT / _RIGHT, in UTF-8).
    float arrow_width = ARROW_WIDTH * block;
    float arrow_y = content_y + std::floor((card - LINE * block) * 0.5f);
    if (widget->scroll > 0.5f)
    {
        retro_ui_layer_rect(margin, content_y, arrow_width, card, with_alpha(palette->panel_dark, LIT_ALPHA));
        retro_ui_layer_text(margin + 3.0f * block, arrow_y, "\xE2\x97\x80", palette->text_highlight);
    }
    if (widget->scroll < max_scroll - 0.5f)
    {
        float arrow_x = margin + inner_width - arrow_width;
        retro_ui_layer_rect(arrow_x, content_y, arrow_width, card, with_alpha(palette->panel_dark, LIT_ALPHA));
        retro_ui_layer_text(arrow_x + 3.0f * block, arrow_y, "\xE2\x96\xB6", palette->text_highlight);
    }
    retro_ui_end_layer();

    if (move_column >= 0)
    {
        move_to(input, widget->picked, move_column - 1);   // column 0 is OUT
    }
    if (close)
    {
        ports_widget_close(widget);
    }
}
