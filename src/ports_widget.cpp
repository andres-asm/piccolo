#include "ports_widget.h"

#include "input_devices.h"
#include "piccolo_text.h"

// The directions and BACK, as bits of PortsWidget::held.
constexpr uint8_t HELD_LEFT  = 1;
constexpr uint8_t HELD_RIGHT = 2;
constexpr uint8_t HELD_UP    = 4;
constexpr uint8_t HELD_DOWN  = 8;
constexpr uint8_t HELD_BACK  = 16;

// ---------------------------------------------------------------------------
// Who is where
// ---------------------------------------------------------------------------

// A controller the view shows: its device connected, not a mouse, not ignored.
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

// A remote pad: whose (the network's peer), or false.
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

static void add_chip(PortsViewColumn* column, const std::vector<PortsViewChip>* before, const InputSystem* input,
                     const InputNet* net, uint32_t controller, const InputDevice* device)
{
    PortsViewChip chip;
    chip.key = controller;
    chip.label = input_controller_label(input, controller);
    uint32_t peer = 0;
    chip.has_ping = remote_peer(net, device->id, &peer);
    // The last ping it had (refreshed every so often).
    for (size_t i = 0; i < before->size(); i++)
    {
        if ((*before)[i].key == controller && !(*before)[i].gone)
        {
            chip.ping_known = (*before)[i].ping_known;
            chip.ping_ms = (*before)[i].ping_ms;
        }
    }
    column->chips.push_back(chip);
}

static bool has_chip(const PortsViewColumn* column, uint32_t key, bool gone)
{
    for (size_t i = 0; i < column->chips.size(); i++)
    {
        if (column->chips[i].key == key && column->chips[i].gone == gone)
        {
            return true;
        }
    }
    return false;
}

// A port's chips again (`port` INPUT_NO_PORT: OUT). Greyed ones stay (until
// the tabs go down) unless that controller is back.
static void rebuild(PortsViewColumn* column, const InputSystem* input, const InputNet* net, int32_t port)
{
    std::vector<PortsViewChip> before;
    before.swap(column->chips);
    if (port >= 0)
    {
        const std::vector<uint32_t>* members = &input->ports[port];
        for (size_t m = 0; m < members->size(); m++)
        {
            const InputDevice* device = shown_device(input, (*members)[m]);
            if (device != nullptr)
            {
                add_chip(column, &before, input, net, (*members)[m], device);
            }
        }
    }
    else
    {
        for (size_t c = 0; c < input->controllers.size(); c++)
        {
            uint32_t controller = static_cast<uint32_t>(c);
            const InputDevice* device = shown_device(input, controller);
            if (device != nullptr && input_controller_port(input, controller) == INPUT_NO_PORT)
            {
                add_chip(column, &before, input, net, controller, device);
            }
        }
    }
    for (size_t i = 0; i < before.size(); i++)
    {
        if (before[i].gone && !has_chip(column, before[i].key, false))
        {
            column->chips.push_back(before[i]);
        }
    }
}

void ports_widget_update(PortsWidget* widget, const InputSystem* input, const InputNet* net, const InputFrame* frame,
                         uint32_t port_count, uint64_t now_ms)
{
    PortsView* view = &widget->view;
    if (port_count == 0)
    {
        if (widget->known || widget->open)
        {
            ports_view_reset(view);
            widget->port_count = 0;
            widget->known = false;
            widget->open = false;
        }
        return;
    }
    port_count = port_count > INPUT_PORT_COUNT ? INPUT_PORT_COUNT : port_count;
    // A new game, or another number of ports: taken as they are, no tabs.
    if (!widget->known || port_count != widget->port_count)
    {
        ports_view_reset(view);
        widget->port_count = port_count;
        widget->known = true;
        view->port_count = port_count;
        for (uint32_t p = 0; p < INPUT_PORT_COUNT; p++)
        {
            rebuild(&view->ports[p], input, net, static_cast<int32_t>(p));
            widget->summary[p] = port_summary(input, p);
        }
        rebuild(&view->out, input, net, INPUT_NO_PORT);
        widget->out_summary = out_summary(input);
        return;
    }
    bool changed[PORTS_VIEW_MAX_PORTS] = {};
    bool any = false;
    for (uint32_t p = 0; p < INPUT_PORT_COUNT; p++)
    {
        uint64_t summary = port_summary(input, p);
        if (summary != widget->summary[p])
        {
            widget->summary[p] = summary;
            rebuild(&view->ports[p], input, net, static_cast<int32_t>(p));
            changed[p] = p < port_count;   // (ports not shown change unseen)
            any = any || changed[p];
        }
    }
    uint64_t out = out_summary(input);
    if (out != widget->out_summary)
    {
        widget->out_summary = out;
        rebuild(&view->out, input, net, INPUT_NO_PORT);
    }
    // A controller whose device went away: greyed on the port it left.
    if (frame->left_port >= 0 && frame->left_port < static_cast<int32_t>(port_count) && frame->left_controller >= 0 &&
        !widget->open)
    {
        uint32_t controller = static_cast<uint32_t>(frame->left_controller);
        PortsViewColumn* column = &view->ports[frame->left_port];
        if (!has_chip(column, controller, true) && controller < input->controllers.size())
        {
            PortsViewChip gone;
            gone.key = controller;
            gone.label = input_controller_label(input, controller);
            gone.gone = true;
            column->chips.push_back(gone);
        }
        changed[frame->left_port] = true;
        any = true;
    }
    if (any && !widget->open)
    {
        ports_view_pop(view, changed, now_ms);
    }
}

bool ports_widget_showing(const PortsWidget* widget)
{
    return widget->open || ports_view_tabs_visible(&widget->view);
}

static void set_pings_in(PortsViewColumn* column, const InputSystem* input, const InputNet* net, const bool* known,
                         const uint32_t* ping_ms)
{
    for (size_t i = 0; i < column->chips.size(); i++)
    {
        PortsViewChip* chip = &column->chips[i];
        uint32_t peer = 0;
        if (!chip->has_ping || chip->gone || chip->key >= input->controllers.size() ||
            !remote_peer(net, input->controllers[chip->key].device_id, &peer))
        {
            continue;
        }
        chip->ping_known = known[peer];
        chip->ping_ms = ping_ms[peer];
    }
}

void ports_widget_set_pings(PortsWidget* widget, const InputSystem* input, const InputNet* net, const bool* known,
                            const uint32_t* ping_ms)
{
    for (uint32_t p = 0; p < INPUT_PORT_COUNT; p++)
    {
        set_pings_in(&widget->view.ports[p], input, net, known, ping_ms);
    }
    set_pings_in(&widget->view.out, input, net, known, ping_ms);
}

float ports_widget_tabs_width(const PortsWidget* widget)
{
    return widget->open ? 0.0f : ports_view_tabs_width(&widget->view);
}

// ---------------------------------------------------------------------------
// Drawing (retroui's ports_view, in piccolo's words)
// ---------------------------------------------------------------------------

static PortsViewText view_text(const Localization* strings)
{
    PortsViewText text;
    text.tab = piccolo_text(strings, Str::ports_tab);
    text.port = piccolo_text(strings, Str::ports_port);
    text.ping = piccolo_text(strings, Str::ports_ping);
    text.nobody = piccolo_text(strings, Str::ports_nobody);
    text.off = piccolo_text(strings, Str::ports_off);
    text.out = piccolo_text(strings, Str::ports_out);
    text.title = piccolo_text(strings, Str::ports_title);
    text.done = piccolo_text(strings, Str::ports_done);
    text.help = piccolo_text(strings, Str::ports_help);
    return text;
}

void ports_widget_draw_tabs(PortsWidget* widget, ImFont* pixel_font, int32_t scale, const Localization* strings,
                            bool menu_shown, uint64_t now_ms)
{
    PortsViewText text = view_text(strings);
    ports_view_draw_tabs(&widget->view, pixel_font, scale, &text, menu_shown, widget->open, now_ms);
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

// A controller on that port only (-1 = on none).
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

// One port to the left (OUT before port 1) or right.
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
    PortsView* view = &widget->view;
    widget->open = true;
    view->popped_ms = 0;
    view->scroll = -1.0f;   // placed at once on the first draw
    // What's held now (the hotkey that opened it) does nothing until let go.
    widget->held.assign(input->controllers.size(), 0);
    for (size_t c = 0; c < input->controllers.size(); c++)
    {
        widget->held[c] = held_bits(input, static_cast<uint32_t>(c));
    }
    // Port 1's first gamepad picked (else whoever is first there).
    view->has_pick = false;
    const std::vector<PortsViewChip>* first = &view->ports[0].chips;
    for (size_t i = 0; i < first->size() && !view->has_pick; i++)
    {
        const InputDevice* device = shown_device(input, (*first)[i].key);
        if (device != nullptr && device->kind == InputDeviceKind::gamepad)
        {
            view->picked = (*first)[i].key;
            view->has_pick = true;
        }
    }
    if (!view->has_pick && !first->empty())
    {
        view->picked = (*first)[0].key;
        view->has_pick = true;
    }
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
    PortsView* view = &widget->view;
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
    int32_t column = 0;
    if (view->has_pick && !ports_view_find(view, view->picked, &column))
    {
        view->has_pick = false;
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
                ports_view_pick_next(view, -1);
            }
            if ((pressed & HELD_DOWN) != 0)
            {
                ports_view_pick_next(view, 1);
            }
            if (direction != 0)
            {
                if (!view->has_pick)
                {
                    view->picked = controller;
                    view->has_pick = true;
                }
                move_by(input, view->picked, direction, widget->port_count);
            }
        }
        else if (direction != 0)
        {
            move_by(input, controller, direction, widget->port_count);
        }
    }
}

void ports_widget_draw_board(PortsWidget* widget, InputSystem* input, ImFont* pixel_font, int32_t scale,
                             const Localization* strings)
{
    if (!widget->open)
    {
        return;
    }
    PortsViewText text = view_text(strings);
    PortsViewBoardInput clicks;
    ports_view_draw_board(&widget->view, pixel_font, scale, &text, &clicks);
    if (clicks.move && clicks.key < input->controllers.size())
    {
        move_to(input, clicks.key, clicks.port);
    }
    if (clicks.done)
    {
        ports_widget_close(widget);
    }
}
