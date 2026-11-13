#pragma once

// The games loaded last, for the RECENT menu: most recent first, at most
// RECENT_GAMES_MAX. Loading one that's already there moves it to the top
// (no duplicates). A core started without content counts too.
//
// Stored in <settings>/recent.cfg (settings_file.h), one section per game:
//   [1]
//   core = /home/ana/.local/share/piccolo/cores/snes9x_libretro.so
//   content = /home/ana/roms/Super Mario World (USA).sfc

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

constexpr uint32_t RECENT_GAMES_MAX = 10;

struct RecentGame
{
    std::string core_path;
    std::string content_path;   // "" = the core without content
};

struct RecentGames
{
    std::vector<RecentGame> games;   // most recent first
};

// Puts this game at the top (moving it if it was further down) and drops
// the oldest beyond RECENT_GAMES_MAX.
void recent_games_add(RecentGames* recent, const char* core_path, const char* content_path);

// A missing file is an empty list (first run). Entries without a core are
// skipped.
bool recent_games_load(RecentGames* recent, const char* path);
bool recent_games_save(const RecentGames* recent, const char* path);

// "Super Mario World (USA)  [snes9x]", or "<no_game>  [snes9x]" for a core
// alone (`no_game` is the caller's text for it).
void recent_games_label(const RecentGame* game, const char* no_game, char* out, size_t out_size);
