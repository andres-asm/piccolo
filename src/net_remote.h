#pragma once

// Settings and actions from paired devices (net_settings.h): piccolo's side.
//
// Settings (FULL access): the ones SETTINGS shows, laid out in its pages
// and sections (GENERAL, INTERFACE, VIDEO > PICTURE / DISPLAY / SYNC, SOUND),
// with labels in the language the device asks for. Not offered: NETWORK (a
// device could cut its own connection) and the folders (paths on this
// machine). A change goes to the layer the device names, as SETTINGS'
// scope; every connected FULL device is told when a value changes, from
// wherever (net_remote_next_change).
//
// Actions: what a device may trigger while a game runs. GAMEPAD access:
// SAVE STATE, LOAD STATE, NEXT / PREVIOUS SLOT, FAST FORWARD (held),
// PAUSE (toggle). FULL access also: RESET, CLOSE GAME. main.cpp carries them
// out with the same functions as the hotkeys and the quick menu.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "localization.h"
#include "net_pairing.h"   // NetAccessLevel, net_access_allows
#include "net_settings.h"
#include "net_trust.h"
#include "settings.h"

struct NetRemote
{
    Localization                  strings;            // in the language last asked for
    uint32_t                      strings_language = 0;   // piccolo_language_code() index loaded in `strings`
    std::vector<NetSetting>       settings;           // described at each first page
    std::vector<NetSettingValues> sent;               // what devices were last told, per offered key
    uint32_t                      sent_revision = 0;  // Settings::revision when `sent` was last compared
    std::string                   sent_core_section;
    std::string                   sent_game_section;
    size_t                        next_check = 0;     // where net_remote_next_change goes on
    bool                          all_changed = false;   // the context changed: one "all" message to send
    std::vector<uint8_t>          reply;              // NET_LINK_MAX_PAYLOAD
};

bool net_remote_init(NetRemote* remote, const Settings* settings);

// What runs, for the actions.
struct NetRemoteGame
{
    bool     running = false;
    uint32_t slot = 1;
    bool     paused = false;   // by a device (PAUSE)
};

// A settings_request / setting_set: the answer into `reply`, its size
// returned; 0 if `data` is neither. `audio_devices`: the playback devices
// connected now, offered as AUDIO DEVICE's choices. `root`: piccolo's
// folder, for the languages.
size_t net_remote_answer_settings(NetRemote* remote, Settings* settings, const char* root,
                                  const std::vector<std::string>* audio_devices, const uint8_t* data, size_t size);

// After settings changed (anywhere): one setting_changed at a time into
// `reply`; call until it returns 0. Cheap when nothing changed (it looks at
// Settings::revision first).
size_t net_remote_next_change(NetRemote* remote, const Settings* settings);

// The actions a device with `access` may trigger now, in `language` (or
// the last one asked for when nullptr), as an actions_result into `reply`.
size_t net_remote_write_actions(NetRemote* remote, uint32_t id, const char* language, const char* root,
                                NetAccessLevel access, const NetRemoteGame* game);

// May a device with `access` trigger `key` now? (The list above, checked
// again when an action arrives.)
bool net_remote_action_allowed(const char* key, NetAccessLevel access, const NetRemoteGame* game);
