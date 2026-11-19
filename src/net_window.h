#pragma once

// piccolo's NETWORK tab in SETTINGS (CONFIG > NETWORK opens SETTINGS on
// it): the listener's settings, this device's key, pairing with its code,
// and the paired devices. It was a window of its own until 2026-11-19
// (Andrés: network settings belong in SETTINGS).
//
//   +- NETWORK (tab) ----------------------------------------------+
//   |   ALLOW CONNECTIONS  [x]                                      |
//   |   NAME               [LIVING ROOM               ]             |
//   |   PORT               [- 55420 +]                              |
//   |   THIS DEVICE        3fa2-91c0-7d4e-b815                       |
//   | -- PAIRING -------------------------------------------------- |
//   |   [PAIR A DEVICE]                                             |
//   |     or, while pairing:                                        |
//   |   CODE  482 915                       (highlight color)       |
//   |   VALIDITY: 4:32  [REFRESH] [STOP]                            |
//   |   Type this code on the other device. It changes after...     |
//   | -- PAIRED DEVICES ------------------------------------------- |
//   |   ANA'S PHONE  9c77-3332-311c-4017  CONNECTED                 |
//   |   [GAMEPAD][STREAM][FULL]  [FORGET]                           |
//   +--------------------------------------------------------------+
//
// The settings rows edit piccolo's global settings; main.cpp restarts the
// listener when ALLOW CONNECTIONS or PORT change. Pairing and the device
// list act on the running NetAccess directly.

#include <cstdint>

#include "localization.h"
#include "net_access.h"
#include "settings.h"

struct NetWindowContext
{
    Settings*           settings = nullptr;
    const Localization* strings = nullptr;
    NetAccess*          access = nullptr;   // nullptr while networking is off
    bool                listen_failed = false;   // on, but the port couldn't be opened
    const char*         this_device = "";   // our key's fingerprint
};

// The tab's contents, inside SETTINGS' tab. The rows always edit the global
// settings (networking isn't per core or game), with labels `label_width`
// wide like the other tabs'.
void net_window_draw_tab(const NetWindowContext* context, float label_width);
