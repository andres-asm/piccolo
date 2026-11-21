#pragma once

// piccolo's side of the ports view (retroui's ports_view: the tabs over the
// game and the DEVICES board, drawn the same as piccoletto's). Here: who is
// on which port, from the input router, and the pads on DEVICES:
//
//   - The columns are rebuilt only when who is where changes (summaries
//     compared every frame, nothing allocated otherwise); a change makes the
//     tabs rise, the changed ports whole; a controller whose device went away
//     is greyed on the port it left.
//   - DEVICES (hotkey.ports) over the paused game: every controller moves
//     itself with LEFT / RIGHT (its own interface bindings, read here: the
//     router lets only port 1 drive the UI); port 1's controllers also pick
//     any controller with UP / DOWN and move that one, and close it with
//     BACK. A mouse picks and moves (ports_view). A move puts the controller
//     on that port only (as the network's DEVICES does). Remote (game_only)
//     pads move from their own device.
//
// Mice aren't shown (one would always sit on port 1).

#include <cstdint>
#include <vector>

#include "input_bindings.h"
#include "input_net.h"
#include "localization.h"
#include "ports_view.h"

struct ImFont;

struct PortsWidget
{
    PortsView view;
    uint32_t  port_count = 0;      // ports shown (0 = no game)
    bool      known = false;       // the summaries were taken for this game
    uint64_t  summary[INPUT_PORT_COUNT] = {};
    uint64_t  out_summary = 0;
    bool      open = false;        // DEVICES
    std::vector<uint8_t> held;     // per controller: its directions and BACK last frame
};

// Every frame, after input_update. `port_count` 0 = no game. `net` may be
// nullptr: its pads are the ones with ping bars.
void ports_widget_update(PortsWidget* widget, const InputSystem* input, const InputNet* net, const InputFrame* frame,
                         uint32_t port_count, uint64_t now_ms);

// The tabs or DEVICES show: the frontend keeps the pings fresh then.
bool ports_widget_showing(const PortsWidget* widget);
// Every remote pad's ping: `known[peer]` / `ping_ms[peer]` (NET_MAX_PEERS each).
void ports_widget_set_pings(PortsWidget* widget, const InputSystem* input, const InputNet* net, const bool* known,
                            const uint32_t* ping_ms);

// How far the tabs reach, in retro pixels (the status line starts past them).
float ports_widget_tabs_width(const PortsWidget* widget);

// The tabs, behind every window; `menu_shown`: they peek.
void ports_widget_draw_tabs(PortsWidget* widget, ImFont* pixel_font, int32_t scale, const Localization* strings,
                            bool menu_shown, uint64_t now_ms);

// DEVICES: open (what's held now is ignored until let go), close.
void ports_widget_open(PortsWidget* widget, const InputSystem* input);
void ports_widget_close(PortsWidget* widget);

// Every frame while open, after input_update: the pads' moves (and port 1's
// BACK, which closes it).
void ports_widget_board_update(PortsWidget* widget, InputSystem* input);

// DEVICES, above everything (and what a mouse did on it).
void ports_widget_draw_board(PortsWidget* widget, InputSystem* input, ImFont* pixel_font, int32_t scale, uint64_t now_ms,
                             const Localization* strings);
