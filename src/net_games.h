#pragma once

// Games from a paired device with FULL access (net_content.h): piccolo's
// answers. The device browses the game folders (SETTINGS > GENERAL), gets
// the cores LOAD GAME would offer, and asks for a game to start; the start
// itself goes through LOAD GAME's path in main.cpp (pending_start), so a
// game from the network starts exactly like one picked here.
//
// An archive asked for whole follows LOAD GAME's rules: the archive itself
// when the core takes archives whole, else the one file inside the core can
// load; several (or none) is refused, the device should open it as a folder.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core_catalog.h"
#include "file_picker.h"
#include "net_content_folders.h"

struct NetGames
{
    NetContentFolders           folders;
    std::vector<std::string>    folders_from;   // the settings `folders` was made from
    std::vector<NetContentCore> cores;          // made again at every first page
    std::vector<uint8_t>        reply;          // NET_LINK_MAX_PAYLOAD, sized once
};

void net_games_init(NetGames* games);

// The game folders as they are in the settings now: made again only when
// they changed (a folder that appeared since counts on the next change, or
// at the next start of the network).
void net_games_set_folders(NetGames* games, const std::vector<std::string>* game_folders);

// A cores_request or list_request: writes the answer into `reply` and
// returns its size. 0 if `data` is neither.
size_t net_games_answer(NetGames* games, const CoreCatalog* catalog, const uint8_t* data, size_t size);

// A load_request: what to start (`choice`) when ok. `id` is the request's,
// for the load_result. False if `data` isn't a load_request.
bool net_games_load(const NetGames* games, const CoreCatalog* catalog, const uint8_t* data, size_t size, uint32_t* id,
                    NetContentStatus* status, FilePickerChoice* choice);
