#pragma once

// The ports over the game (Andrés, 2026-11-20; piccoletto's look, its
// DEVICES and its port LEDs and ping):
//
//   TABS     one tab per port shown, all the same width, at the bottom left.
//            When a controller joins, leaves or disconnects, every tab rises
//            to its first line (the port's LEDs, P1..., a remote pad's ping)
//            and the port that changed rises whole (who is on it); about 3 s
//            later they go down again. With the menu open they peek: the
//            first line only. Animated (they slide), over the game but behind
//            every window. More than 4 ports: they overlap.
//   DEVICES  the hotkey (hotkey.ports) opens it full screen over the paused
//            game: a column per port plus OUT (connected controllers on none).
//            Every controller moves itself with LEFT / RIGHT (its own interface
//            bindings, read here: the router lets only port 1 drive the UI);
//            port 1's controllers also pick any controller with UP / DOWN and
//            move that one, and close it with BACK. A mouse can pick (click)
//            and move (click a column). Scrolls sideways when the columns
//            don't fit. A move puts the controller on that port only (as the
//            network's DEVICES does): input_port_add / input_port_remove.
//
// Mice aren't shown (one would always sit on port 1). Nothing allocates per
// frame: what the tabs show is rebuilt only when a port changes.

#include <cstdint>
#include <string>
#include <vector>

#include "input_bindings.h"
#include "input_net.h"
#include "localization.h"

struct ImFont;

struct PortsWidgetDevice
{
    uint32_t    controller = 0;
    std::string label;             // input_controller_label
    bool        gone = false;      // its device went away (shown until the tabs go down)
    bool        remote = false;    // a paired device's pad
    uint32_t    peer = 0;          // remote: whose (the network's peer)
    bool        ping_known = false;
    uint32_t    ping_ms = 0;       // remote: the round trip, set by the frontend (ports_widget_set_ping)
};

struct PortsWidgetColumn
{
    std::vector<PortsWidgetDevice> devices;   // in the port's order; gone ones last
    uint64_t summary = 0;                     // who is on it (compared every frame)
};

struct PortsWidget
{
    // What runs: the ports shown (0 = no game: everything hidden and forgotten).
    uint32_t          port_count = 0;
    bool              known = false;    // the summaries were taken for this game
    PortsWidgetColumn ports[INPUT_PORT_COUNT];
    PortsWidgetColumn out;              // connected controllers on no port (DEVICES' OUT)

    // TABS.
    uint64_t popped_ms = 0;                       // when they rose for a change (0 = not up)
    bool     changed[INPUT_PORT_COUNT] = {};      // these rise whole
    float    height[INPUT_PORT_COUNT] = {};       // how far each has risen now (retro pixels; animated)
    float    target[INPUT_PORT_COUNT] = {};       // where each is going
    uint64_t target_ms[INPUT_PORT_COUNT] = {};    // when that target was set (a short stagger per tab)
    uint64_t last_draw_ms = 0;

    // DEVICES.
    bool     open = false;
    uint32_t picked = UINT32_MAX;       // port 1's pick: a controller
    std::vector<uint8_t> held;          // per controller: its directions and BACK last frame
    float    scroll = 0.0f;             // window pixels (animated toward scroll_target)
    float    scroll_target = 0.0f;
};

// Every frame, after input_update: who is on which port. `port_count` 0 = no
// game. `net` may be nullptr (no network): its pads are marked remote.
void ports_widget_update(PortsWidget* widget, const InputSystem* input, const InputNet* net, const InputFrame* frame,
                         uint32_t port_count, uint64_t now_ms);

// The tabs are (or are going) up: the frontend keeps the pings fresh then.
bool ports_widget_tabs_visible(const PortsWidget* widget);
// A remote peer's round trip, for every pad of that peer shown.
void ports_widget_set_ping(PortsWidget* widget, uint32_t peer, bool known, uint32_t ping_ms);

// How wide the tabs are, in retro pixels from the left edge (0 = none): the
// status line starts past them.
float ports_widget_tabs_width(const PortsWidget* widget);

// The tabs, behind every window. `menu_shown`: they peek.
void ports_widget_draw_tabs(PortsWidget* widget, ImFont* pixel_font, int32_t scale, const Localization* strings,
                            bool menu_shown, uint64_t now_ms);

// DEVICES: open (what's held now is ignored until let go), close.
void ports_widget_open(PortsWidget* widget, const InputSystem* input);
void ports_widget_close(PortsWidget* widget);

// Every frame while open, after input_update: the controllers' moves (and
// port 1's BACK, which closes it).
void ports_widget_board_update(PortsWidget* widget, InputSystem* input);

// DEVICES, above everything (and its mouse: a click picks, a click on a
// column moves the pick there, DONE closes).
void ports_widget_draw_board(PortsWidget* widget, InputSystem* input, ImFont* pixel_font, int32_t scale,
                             const Localization* strings);
