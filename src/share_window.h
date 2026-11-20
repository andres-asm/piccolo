#pragma once

// SHARE: copies games, saves and states between this piccolo and another one
// that gave us OWNER access (piccolo-share), in the retro style:
//
//   +- SHARE --------------------------------------------------------+
//   | THIS PICCOLO                        LIVING ROOM PC *            |
//   | [ roms                        v ]   [ ROMS                  v ] |
//   | roms/gba                            ROMS/gba                    |
//   | +- folders -------------------+     +- folders --------------+  |
//   | | ..                          |     | ..                     |  |
//   | +-----------------------------+ [>] +------------------------+  |
//   | +- files ---------------------+ [<] +- files ----------------+  |
//   | | Metroid Fusion.gba    8 MIB |     | Zero Mission.gba 8 MIB |  |
//   | +-----------------------------+     +------------------------+  |
//   | SENDING METROID FUSION.GBA [####    ] 3.1 / 8.0 MIB   [CANCEL] |
//   +-----------------------------------------------------------------+
//
// Each side: its shared folders in the combo (game folders, SAVES, STATES),
// where we are, folders on top and files below. The arrows in the middle
// (retro_ui_arrow_button) copy the selected file into the folder open on
// the other side. This side is this piccolo's own shared folders
// (net_share_folders.h, read here directly); the other side is what its
// list_results say, asked for and handed in by the caller
// (share_window_remote_wants_list / share_window_set_remote_list). Copies
// asked for go into `copies` for the caller to run; the line at the bottom
// shows the one running (ShareTransferLine).

#include <cstdint>
#include <string>
#include <vector>

#include "localization.h"
#include "net_share.h"
#include "net_share_folders.h"
#include "net_share_link.h"

struct ImFont;

struct SharePane
{
    std::string                path;   // virtual: "" = the shared folders, "ROMS/gba" = inside one
    std::vector<NetShareEntry> entries;
    NetShareStatus             status = NetShareStatus::ok;
    bool                       stale = true;     // list again
    bool                       waiting = false;  // the other side: asked, no answer yet
    int32_t                    selected = -1;    // a file in `entries`
};

// One copy asked for, in the window's words: from a virtual path on one
// side into a folder on the other.
struct ShareCopy
{
    bool        to_remote = true;   // this piccolo -> the other one
    std::string from;               // "roms/gba/Metroid Fusion.gba"
    std::string to;                 // "ROMS/gba/Metroid Fusion.gba"
    uint64_t    size = 0;
};

// What the bottom line shows (from the caller: the copy running).
struct ShareTransferLine
{
    bool        active = false;
    bool        sending = true;   // this piccolo sends (or receives)
    bool        paused = false;   // someone plays remotely: copies wait
    std::string name;
    uint64_t    done = 0;
    uint64_t    size = 0;
    uint32_t    rate_kbps = 0;
    uint32_t    queued = 0;       // copies after this one
};

struct ShareWindow
{
    bool      open = false;
    SharePane local;
    SharePane remote;
    std::vector<NetShareEntry> remote_roots;   // the other side's shared folders (its root listing)
    std::vector<NetShareEntry> local_roots;    // ours, read with this side's listing
    std::string message;             // what the last copy said, when it didn't go well

    // Connecting (the other side, while no piccolo is connected with OWNER).
    int32_t found_selected = -1;     // in the link's found list
    char    code[8] = {};            // the 6 digits it shows, only the first time
    bool    connect = false;         // CONNECT clicked: the caller connects
    bool    disconnect = false;      // DISCONNECT clicked
    std::vector<ShareCopy> copies;   // asked for, taken by the caller
    bool      cancel = false;        // CANCEL clicked: the caller stops the copy running
};

struct ShareWindowContext
{
    const Localization*      strings = nullptr;
    const NetShareFolders*   local_folders = nullptr;   // this piccolo's shared folders
    const NetShareLinkStatus* link = nullptr;           // our connection to another piccolo (nullptr: none)
    const ShareTransferLine* line = nullptr;
};

void share_window_open(ShareWindow* window);
void share_window_draw(ShareWindow* window, const ShareWindowContext* context, ImFont* pixel_font, int32_t scale);

// The other side's folder to list, once per change (the caller sends a
// list_request for it and hands the answer to share_window_set_remote_list).
bool share_window_remote_wants_list(ShareWindow* window, std::string* path);
void share_window_set_remote_list(ShareWindow* window, const char* path, NetShareStatus status,
                                  const std::vector<NetShareEntry>* entries);
// The other piccolo left (or a new one came): its side starts over.
void share_window_remote_reset(ShareWindow* window);
