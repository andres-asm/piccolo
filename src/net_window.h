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
//   |   INTERNET           [x]                                      |
//   |   BEACON SERVER      [beacon.get-piccolo.app    ]             |
//   |   INTERNET STATUS    READY: PAIRED DEVICES CAN CONNECT        |
//   |   THIS DEVICE        3fa2-91c0-7d4e-b815                       |
//   | -- PAIRING -------------------------------------------------- |
//   |   [PAIR A DEVICE]                                             |
//   |     or, while pairing:                                        |
//   |   CODE  482 915                       (highlight color)       |
//   |   VALIDITY: 4:32  [REFRESH] [STOP]  [COPY LINK]               |
//   |   ######  (INTERNET on: the pair link as a QR, for a device    |
//   |   ######   on another network: scan it, then type the code)   |
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
#include <string>
#include <vector>

#include "localization.h"
#include "net_access.h"
#include "settings.h"

// The last speed test of a device, as it reported each step (net_speed.h
// speed_result): the best rate each way that lost no more than
// NET_WINDOW_SPEED_MAX_LOSS. Kept while piccolo runs, by device key.
constexpr uint16_t NET_WINDOW_SPEED_MAX_LOSS = 20;   // per mille

struct NetSpeedSummary
{
    uint8_t  key[NET_KEY_SIZE] = {};
    uint32_t down_kbps = 0;   // 0 = none clean (or not tested)
    uint32_t up_kbps = 0;
    uint32_t down_last_asked = 0;   // a step asking less than the last starts a new test
    uint32_t up_last_asked = 0;
    bool     down_tested = false;
    bool     up_tested = false;
};

// What the tab keeps between frames: OWNER is given only after its
// confirmation, to the device with this key (the list may change meanwhile).
// The pair link's QR (qrcodegen) is made again only when the link changes.
constexpr int32_t  NET_WINDOW_QR_MAX_VERSION = 10;   // 57 x 57 modules; the link needs about version 6
constexpr uint32_t NET_WINDOW_QR_BUFFER =
    ((NET_WINDOW_QR_MAX_VERSION * 4 + 17) * (NET_WINDOW_QR_MAX_VERSION * 4 + 17) + 7) / 8 + 1;

struct NetWindowState
{
    uint8_t owner_key[NET_KEY_SIZE] = {};
    bool    confirm_owner = false;
    std::string qr_link;             // the pair link `qr` encodes ("" = nothing yet)
    std::string qr_beacon;           // made for this beacon setting
    uint8_t qr_key[NET_KEY_SIZE] = {};   // and this host key
    bool    qr_ok = false;
    uint8_t qr[NET_WINDOW_QR_BUFFER] = {};
};

struct NetWindowContext
{
    NetWindowState*     state = nullptr;
    Settings*           settings = nullptr;
    const Localization* strings = nullptr;
    NetAccess*          access = nullptr;   // nullptr while networking is off
    bool                listen_failed = false;   // on, but the port couldn't be opened
    const char*         this_device = "";   // our key's fingerprint
    const std::vector<NetSpeedSummary>* speed = nullptr;   // speed test results, by device key
    bool                internet = false;   // INTERNET is on (the beacon status row shows)
};

// The tab's contents, inside SETTINGS' tab. The rows always edit the global
// settings (networking isn't per core or game), with labels `label_width`
// wide like the other tabs'.
void net_window_draw_tab(const NetWindowContext* context, float label_width);
