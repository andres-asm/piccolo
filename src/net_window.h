#pragma once

// piccolo's NETWORK window (CONFIG > NETWORK): the listener's settings, this
// device's key, pairing with its code, and the paired devices.
//
//   +- NETWORK ----------------------------------------------------+
//   |   ALLOW CONNECTIONS  [x]                                      |
//   |   NAME               [LIVING ROOM               ]             |
//   |   PORT               [- 55420 +]                              |
//   |   THIS DEVICE        3fa2-91c0-7d4e-b815                       |
//   | -- PAIRING -------------------------------------------------- |
//   |   [PAIR A DEVICE]                                             |
//   |     or, while pairing:                                        |
//   |   CODE   482 915            4:32 LEFT   [NEW CODE] [STOP]     |
//   |   Type this code on the other device. It changes after...     |
//   | -- PAIRED DEVICES ------------------------------------------- |
//   |   ANA'S PHONE   9c77-3332-311c-4017  [x] OWN  CONNECTED [FORGET]
//   +--------------------------------------------------------------+
//
// The settings rows edit piccolo's global settings; main.cpp restarts the
// listener when ALLOW CONNECTIONS or PORT change. Pairing and the device
// list act on the running NetAccess directly.

#include <cstdint>

#include "localization.h"
#include "net_access.h"
#include "settings.h"

struct ImFont;

struct NetWindow
{
    bool open = false;
};

struct NetWindowContext
{
    Settings*           settings = nullptr;
    const Localization* strings = nullptr;
    NetAccess*          access = nullptr;   // nullptr while networking is off
    bool                listen_failed = false;   // on, but the port couldn't be opened
    const char*         this_device = "";   // our key's fingerprint
};

void net_window_open(NetWindow* window);
void net_window_draw(NetWindow* window, const NetWindowContext* context, ImFont* pixel_font, int32_t scale);
